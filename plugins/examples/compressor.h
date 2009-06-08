
/* Copyright (c) 2009, Cedric Stalder <cedric.stalder@gmail.com> 
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
 
#ifndef EQ_PLUGIN_COMPRESSOR
#define EQ_PLUGIN_COMPRESSOR 

#define EQ_PLUGIN

#include <eq/plugin/compressor.h>
#include <eq/base/base.h>
#include <eq/client/compressor.h>

/**
 * @file examples/compressor.h
 * 
 * Compression plugin provided with Equalizer.
 */

namespace eq
{
namespace plugin
{
    typedef void  (*CompressorGetInfo_t)( EqCompressorInfo* const );
    typedef void* (*NewCompressor_t)();

    struct Functions
    {
        Functions();

        CompressorGetInfo_t      getInfo;
        NewCompressor_t          newCompressor;
    };
    

    typedef base::Bufferb Result;

     /**
     * An interace for compressor / uncompressor data
     *
     */
    class Compressor
    {
    public:
        /**
         * Construct a new compressor.
         *
         * @param buffer the number channel.
         */
        Compressor( const uint32_t numChannel );

        virtual ~Compressor();

        /** @name compress */
        /*@{*/
        /**
         * compress Data.
         *
         * @param data to compress.
         * @param number data to compress.
         * @param use alpha channel in compression.
         */
        virtual void compress( void* const inData, 
                               const uint64_t inSize, 
                               const bool useAlpha ) = 0;

        /** @name decompress */
        /*@{*/
        /**
         * uncompress Data.
         *
         * @param data(s) to compress.
         * @param size(s)of the data to compress.
         * @param result of uncompressed data.
         * @param size of the result.
         */
        virtual void decompress( const void** const inData, 
                                 const uint64_t* const inSizes, 
                                 void* const outData, 
                                 const uint64_t* const outSize )=0;

        /** @name getResults */
        /*@{*/
        /**
         * get the number results that compression use to save data
         */
        std::vector< Result* >& getResults(){ return _results; }

        /** @name getName */
        /*@{*/
        /**
         * get the compressor's name
         */
        unsigned getName(){ return _name; }

    protected: 
      std::vector< Result* > _results;  //!< The compressed data
      unsigned _name;                   // name compressor
      const uint32_t _numChannels;      // number channel
      bool _swizzleData;                // using swizzle data
    }; 
}
}

#endif
