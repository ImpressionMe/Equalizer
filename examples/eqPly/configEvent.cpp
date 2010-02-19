
/* Copyright (c) 2009-2010, Stefan Eilemann <eile@equalizergraphics.com>
 *               2009, Sarah Amsellem <sarah.amsellem@gmail.com>
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

#include "configEvent.h"

namespace eqPly
{
std::ostream& operator << ( std::ostream& os, const ConfigEvent* event )
{
    switch( event->data.type )
    {
        case ConfigEvent::IDLE_AA_LEFT:
            os  << event->steps << " FSAA steps to do";
            break;

        default:
            os << static_cast< const eq::ConfigEvent* >( event );
            return os;
    }

    return os;
}
}
