/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Zhur
*/

#include "eve-core.h"

#include "network/StreamPacketizer.h"

#include <cstring>

StreamPacketizer::~StreamPacketizer()
{
    ClearBuffers();
}

void StreamPacketizer::InputData( const Buffer& data )
{
    mBuffer.AppendSeq( data.begin<uint8>(), data.end<uint8>() );
}

void StreamPacketizer::Process()
{
    Buffer::const_iterator<uint8> cur = mBuffer.begin<uint8>(), end = mBuffer.end<uint8>();
    while( true ) {
        if (sizeof(uint32) > (end - cur))
            break;

        // Frame boundaries can be at arbitrary byte offsets. Never read a
        // uint32 directly through a possibly unaligned Buffer iterator.
        uint32 packetLength = 0;
        std::memcpy(&packetLength, &*cur, sizeof(packetLength));
        const Buffer::const_iterator<uint8> start = cur + sizeof(uint32);

        if (packetLength > static_cast<uint32>(end - start))
            break;

        mPackets.push(new Buffer(start, start + packetLength));
        cur = start + packetLength;
    }

    if( cur != mBuffer.begin<uint8>() )
        mBuffer.AssignSeq( cur, end );
}

Buffer* StreamPacketizer::PopPacket()
{
    Buffer* buf(nullptr);
    if (!mPackets.empty()) {
        buf = mPackets.front();
        mPackets.pop();
    }

    return buf;
}

void StreamPacketizer::ClearBuffers()
{
    Buffer* buf(nullptr);
    while ((buf = PopPacket()))
        SafeDelete( buf );
    // Also discard any incomplete frame left on this connection.
    mBuffer.Resize<uint8>(0);
}
