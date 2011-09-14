
/* Copyright (c) 2007-2011, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "compoundUpdateOutputVisitor.h"

#include "config.h"
#include "frame.h"
#include "frameData.h"
#include "server.h"
#include "tileQueue.h"
#include "window.h"

#include <eq/client/log.h>
#include <eq/fabric/iAttribute.h>

#define TILE_STRATEGY STGY_TILE_ZIGZAG

namespace eq
{
namespace server
{
CompoundUpdateOutputVisitor::CompoundUpdateOutputVisitor(  
    const uint32_t frameNumber )
        : _frameNumber( frameNumber )
{}

VisitorResult CompoundUpdateOutputVisitor::visit( Compound* compound )
{
    if( !compound->isRunning( ))
        return TRAVERSE_PRUNE;    

    _updateOutput( compound );
    _updateSwapBarriers( compound );

    return TRAVERSE_CONTINUE;    
}

void CompoundUpdateOutputVisitor::_updateOutput( Compound* compound )
{
    const Channel* channel = compound->getChannel();

    const TileQueues& outputQueues = compound->getOutputTileQueues();
    for( TileQueuesCIter i = outputQueues.begin(); 
        i != outputQueues.end(); ++i )
    {
        //----- Check uniqueness of output queue name
        TileQueue* queue  = *i;
        const std::string& name   = queue->getName();

        if( _outputTileQueues.find( name ) != _outputTileQueues.end())
        {
            EQWARN << "Multiple output queues of the same name are unsupported"
                << ", ignoring output queue " << name << std::endl;
            queue->unsetData();
            continue;
        }

        queue->cycleData( _frameNumber, compound );

        //----- Generate tile task packets
        TileStrategy strategy = static_cast <TileStrategy> ( TILE_STRATEGY );
        _generateTiles( queue, compound, strategy );

        _outputTileQueues[name] = queue;
    }

    if( !compound->testInheritTask( fabric::TASK_READBACK ) || !channel )
        return;

    const Frames& outputFrames = compound->getOutputFrames();
    if( outputFrames.empty( ))
    {
        compound->unsetInheritTask( fabric::TASK_READBACK );
        return;
    }

    for( Frames::const_iterator i = outputFrames.begin(); 
         i != outputFrames.end(); ++i )
    {
        //----- Check uniqueness of output frame name
        Frame*             frame  = *i;
        const std::string& name   = frame->getName();

        if( _outputFrames.find( name ) != _outputFrames.end())
        {
            EQWARN << "Multiple output frames of the same name are unsupported"
                   << ", ignoring output frame " << name << std::endl;
            frame->unsetData();
            continue;
        }

        //----- compute readback area
        const Viewport& frameVP = frame->getViewport();
        const PixelViewport& inheritPVP = compound->getInheritPixelViewport();
        PixelViewport framePVP( inheritPVP );
        framePVP.apply( frameVP );
        
        if( !framePVP.hasArea( )) // output frame has no pixels
        {
            EQINFO << "Skipping output frame " << name << ", no pixels"
                   << std::endl;
            frame->unsetData();
            continue;
        }

        //----- Create new frame datas
        //      one frame data used for each eye pass
        //      data is set only on master frame data (will copy to all others)
        frame->cycleData( _frameNumber, compound );
        FrameData* frameData = frame->getMasterData();
        EQASSERT( frameData );

        EQLOG( LOG_ASSEMBLY )
            << co::base::disableFlush << "Output frame \"" << name << "\" id " 
            << frame->getID() << " v" << frame->getVersion()+1
            << " data id " << frameData->getID() << " v" 
            << frameData->getVersion() + 1 << " on channel \""
            << channel->getName() << "\" tile pos " << framePVP.x << ", " 
            << framePVP.y;

        //----- Set frame data parameters:
        // 1) offset is position wrt destination view
        
        bool usesTiles = !compound->getInputTileQueues().empty();
        frameData->setOffset( usesTiles ? Vector2i( 0 , 0 ) :
                                          Vector2i( framePVP.x, framePVP.y ) );

        // 2) pvp is area within channel
        framePVP.x = static_cast< int32_t >( frameVP.x * inheritPVP.w );
        framePVP.y = static_cast< int32_t >( frameVP.y * inheritPVP.h );
        frameData->setPixelViewport( framePVP );

        // 3) image buffers and storage type
        uint32_t buffers = frame->getBuffers();

        frameData->setType( frame->getType() );
        frameData->setBuffers( buffers == eq::Frame::BUFFER_UNDEFINED ? 
                                   compound->getInheritBuffers() : buffers );

        // 4) (source) render context
        frameData->setRange( compound->getInheritRange( ));
        frameData->setPixel( compound->getInheritPixel( ));
        frameData->setSubPixel( compound->getInheritSubPixel( ));
        frameData->setPeriod( compound->getInheritPeriod( ));
        frameData->setPhase( compound->getInheritPhase( ));

        //----- Set frame parameters:
        // 1) offset is position wrt window, i.e., the channel position
        if( compound->getInheritChannel() == channel )
            frame->setInheritOffset( Vector2i( inheritPVP.x, inheritPVP.y ));
        else
        {
            const PixelViewport& nativePVP = channel->getPixelViewport();
            frame->setInheritOffset( Vector2i( nativePVP.x, nativePVP.y ));
        }

        // 2) zoom
        _updateZoom( compound, frame );

#if 0
        //@bug? Where did this go? ----- Commit
        // https://github.com/tribal-tec/Equalizer/commit/0e016a608f403f0234c3aba7ddb4a6906d1b219f
        // moved to compoundUpdateInputVisitor.cpp:135, commit needs to be done
        // after setting the input nodes to the outputframe
        // follow outputFrame->addInputFrame( frame, compound );
        frame->commitData();
        frame->commit();
#endif
        _outputFrames[name] = frame;
        EQLOG( LOG_ASSEMBLY ) 
            << " buffers " << frameData->getBuffers() << " read area "
            << framePVP << " readback " << frame->getInheritZoom()
            << " assemble " << frameData->getZoom() << std::endl
            << co::base::enableFlush;
   } 
}

void CompoundUpdateOutputVisitor::_generateTiles( TileQueue* queue,
                                                  Compound* compound,
                                                  TileStrategy strategy )
{
    const Vector2i& tileSize = queue->getTileSize();
    PixelViewport pvp = compound->getInheritPixelViewport();
    if( !pvp.hasArea( ))
        return;

    Vector2i dim;
    dim.x() = ( pvp.w/tileSize.x() + ((pvp.w%tileSize.x())? 1 : 0));
    dim.y() = ( pvp.h/tileSize.y() + ((pvp.h%tileSize.y())? 1 : 0));

    std::vector< Vector2i > tileOrder;
    // generate tiles order according with the chosen strategy
    _applyTileStrategy( tileOrder, dim, strategy );
   
    // add tiles packets ordered with strategy
    _addTilesToQueue( queue, compound, tileOrder );

}

void CompoundUpdateOutputVisitor::_applyTileStrategy(
                                            std::vector< Vector2i >& tileOrder,
                                            const Vector2i& dim,
                                            TileStrategy strategy )
{
    tileOrder.reserve( dim.x() * dim.y() );

    switch(strategy)
    {
        case STGY_TILE_CWSPIRAL:
            _spiralStrategy( tileOrder, dim ); 
            break;

        case STGY_TILE_SQSPIRAL:
            _squareStrategy( tileOrder, dim ); 
            break;

        case STGY_TILE_ZIGZAG:
            _zigzagStrategy( tileOrder, dim );
            break;

        default:
        /* FALL BACK */
        case STGY_TILE_RASTER:
            _rasterStrategy( tileOrder, dim );
            break;
    }
}

void CompoundUpdateOutputVisitor::_rasterStrategy( 
                                            std::vector< Vector2i >& tileOrder,
                                            const Vector2i& dim ) 
{
    Vector2i tile;

    for (int y = 0; y < dim.y(); ++y)
    {
        for (int x = 0; x < dim.x(); ++x)
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }
    }
}

void CompoundUpdateOutputVisitor::_zigzagStrategy( 
                                            std::vector< Vector2i >& tileOrder,
                                            const Vector2i& dim ) 
{
    Vector2i tile;

    for (int y = 0; y < dim.y(); ++y)
    {
        if( y%2 )
        {
            for (int x = dim.x()-1; x >= 0; --x)
            {
                tile.x() = x;
                tile.y() = y;
                tileOrder.push_back(tile);
            }
        }
        else
        {
            for (int x = 0; x < dim.x(); ++x)
            {
                tile.x() = x;
                tile.y() = y;
                tileOrder.push_back(tile);
            }
        }
    }
}


void CompoundUpdateOutputVisitor::_spiralStrategy( 
                                            std::vector< Vector2i >& tileOrder,
                                            const Vector2i& dim ) 
{
    Vector2i tile;

    int dimX = dim.x();
    int dimY = dim.y();

    int level = ( dimY < dimX ? dimY-1 : dimX-1 ) / 2;

    int x(0);
    int y(0);

    while ( level >= 0 )
    {

        for(x = level,y = level+1; y < dimY-level; ++y)
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }

        for(x = level+1, y = dimY-1-level; x < dimX-1-level; ++x)
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }

        for(x = dimX-1-level, y = dimY-1-level; y > level ; --y )
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }

        for(x = dimX-1-level, y = level; x > level-1 ; --x )
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }
        --level;
    }
}

void CompoundUpdateOutputVisitor::_squareStrategy( 
                                            std::vector< Vector2i >& tileOrder,
                                            const Vector2i& dim ) 
{
    Vector2i tile;
   
    int dimX = dim.x();
    int dimY = dim.y();
    uint32_t dimXY = dim.x() * dim.y();
    int x = ( dim.x() - 1 ) / 2;
    int y = ( dim.y() - 1 ) / 2;
    int walk = 0;
    int radius = 0;
    int xStep = 0;
    int yStep = -1; //down

    while ( tileOrder.size() < dimXY )
    {
        EQASSERT ( (yStep == 0 && xStep != 0) || (yStep != 0 && xStep == 0) );
        if ( x>=0 && x<dimX && y>=0 && y<dimY )
        {
            tile.x() = x;
            tile.y() = y;
            tileOrder.push_back(tile);
        }

        ++walk;
        if ( walk < radius )
        {
            // keep direction
            x += xStep;
            y += yStep;
        }
        else // change direction
        {
            if ( xStep == 1 && yStep == 0 )
            {
                // right -> up
                xStep = 0;
                yStep = 1;
            }
            else if ( xStep == 0 && yStep == 1  )
            {
                // up -> left
                xStep = -1;
                yStep = 0;
                ++radius; //
            }
            else if ( xStep == -1 && yStep == 0 )
            {
                // left -> down
                xStep = 0;
                yStep = -1;
            }
            else // ( xStep == 0 && yStep == -1 )
            {
                // down -> right
                xStep = 1;
                yStep = 0;
                ++radius; //
            }
            walk = 0;
            x += xStep;
            y += yStep;
        }
    }
}

void CompoundUpdateOutputVisitor::_addTilesToQueue( TileQueue* queue, 
                                            Compound* compound, 
                                            std::vector< Vector2i >& tileOrder )
{

    const Vector2i& tileSize = queue->getTileSize();
    PixelViewport pvp = compound->getInheritPixelViewport();

    double xFraction = 1.0 / pvp.w;
    double yFraction = 1.0 / pvp.h;
    float vpWidth = tileSize.x() * xFraction;
    float vpHeight = tileSize.y() * yFraction;

    std::vector< Vector2i >::iterator tileItr;
    for( tileItr = tileOrder.begin(); tileItr != tileOrder.end(); ++tileItr )
    {
        PixelViewport tilepvp;
        Viewport tilevp;

        tilepvp.x = tileItr->x() * tileSize.x();
        tilepvp.y = tileItr->y() * tileSize.y();
        tilevp.x = tilepvp.x * xFraction;
        tilevp.y = tilepvp.y * yFraction;

        if ( tilepvp.x + tileSize.x() <= pvp.w )
        {
            tilepvp.w = tileSize.x();
            tilevp.w = vpWidth;
        }
        else
        {
            // no full tile
            tilepvp.w = pvp.w  - tilepvp.x;
            tilevp.w = tilepvp.w * xFraction;
        }

        if ( tilepvp.y + tileSize.y() <= pvp.h )
        {
            tilepvp.h = tileSize.y();
            tilevp.h = vpHeight;
        }
        else
        {
            // no full tile
            tilepvp.h = pvp.h - tilepvp.y;
            tilevp.h = tilepvp.h * yFraction;
        }

        fabric::Eye eye = fabric::EYE_CYCLOP;
        for ( ; eye < fabric::EYES_ALL; eye = fabric::Eye(eye<<1) )
        {
            if ( !(compound->getInheritEyes() & eye) ||
                !compound->isInheritActive( eye ))
                continue;

            TileTaskPacket packet;
            packet.pvp = tilepvp;
            packet.vp = tilevp;

            compound->computeTileFrustum( packet.frustum, 
                eye, packet.vp, false );
            compound->computeTileFrustum( packet.ortho, 
                eye, packet.vp, true );
            queue->addTile( packet, eye );
        }
    }
}


void CompoundUpdateOutputVisitor::_updateZoom( const Compound* compound,
                                               Frame* frame )
{
    Zoom zoom = frame->getZoom();
    Zoom zoom_1;

    if( !zoom.isValid( )) // if zoom is not set, auto-calculate from parent
    {
        zoom_1 = compound->getInheritZoom();
        EQASSERT( zoom_1.isValid( ));
        zoom.x() = 1.0f / zoom_1.x();
        zoom.y() = 1.0f / zoom_1.y();
    }
    else
    {
        zoom_1.x() = 1.0f / zoom.x();
        zoom_1.y() = 1.0f / zoom.y();
    }

    if( frame->getType( ) == eq::Frame::TYPE_TEXTURE )
    {
        FrameData* frameData = frame->getMasterData();
        frameData->setZoom( zoom_1 ); // textures are zoomed by input frame
        frame->setInheritZoom( Zoom::NONE );
    }
    else
    {
        Zoom inputZoom;
        /* Output frames downscale pixel data during readback, and upscale it on
         * the input frame by setting the input frame's inherit zoom. */
        if( zoom.x() > 1.0f )
        {
            inputZoom.x() = zoom_1.x();
            zoom.x()      = 1.f;
        }
        if( zoom.y() > 1.0f )
        {
            inputZoom.y() = zoom_1.y();
            zoom.y()      = 1.f;
        }

        FrameData* frameData = frame->getMasterData();
        frameData->setZoom( inputZoom );
        frame->setInheritZoom( zoom );                
    }
}

void CompoundUpdateOutputVisitor::_updateSwapBarriers( Compound* compound )
{
    SwapBarrierConstPtr swapBarrier = compound->getSwapBarrier();
    if( !swapBarrier )
        return;

    Window* window = compound->getWindow();
    EQASSERT( window );
    if( !window )
        return;

    if( swapBarrier->isNvSwapBarrier( ))
    {
        if( !window->hasNVSwapBarrier( ))
        {
            const std::string name( "__NV_swap_group_protection_barrier__" );
            _swapBarriers[name] = 
                window->joinNVSwapBarrier( swapBarrier, _swapBarriers[name] );
        }
    }
    else
    {
        const std::string& name = swapBarrier->getName();
        _swapBarriers[name] = window->joinSwapBarrier( _swapBarriers[name] );
    }
}

}
}

