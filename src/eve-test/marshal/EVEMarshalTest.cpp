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
    Author:     Bloody.Rabbit
*/

#include "eve-test.h"
#include "python/PyPacket.h"

#include <cstring>

namespace {
// Exercised by the existing EVEMarshalTest across GCC, MinGW and sanitizers.
// The original PyObject copy constructor shallow-copied both owned pointers,
// causing the clone to delete live source objects during packet encoding.
bool WireObjectOwnershipRegression()
{
    PyObject* source = new PyObject("util.KeyVal", new_tuple(new PyInt(71)));
    PyObject* clone = source->Clone()->AsObject();
    PyDecRef(source);
    const bool cloneSurvives = clone->type() != nullptr &&
        clone->type()->content() == "util.KeyVal" &&
        clone->arguments() != nullptr &&
        clone->arguments()->AsTuple()->size() == 1 &&
        clone->arguments()->AsTuple()->GetItem(0)->AsInt()->value() == 71;
    PyDecRef(clone);
    if (!cloneSurvives)
        return false;

    PyTuple* tuple = new PyTuple(1);
    tuple->SetItem(0, new PyInt(5));
    *tuple = *tuple;
    const bool tupleStable = tuple->size() == 1 &&
        tuple->GetItem(0)->AsInt()->value() == 5;
    PyDecRef(tuple);

    PyList* list = new PyList();
    list->AddItemInt(6);
    *list = *list;
    const bool listStable = list->size() == 1 &&
        list->GetItem(0)->AsInt()->value() == 6;
    PyDecRef(list);

    PyDict* dict = new PyDict();
    dict->SetItemString("value", new PyInt(7));
    *dict = *dict;
    PyRep* value = dict->GetItemString("value");
    const bool dictStable = value != nullptr && value->IsInt() &&
        value->AsInt()->value() == 7;
    PyDecRef(dict);

    PyObjectEx* extended = new PyObjectEx(true, new PyTuple(0));
    extended->list().AddItemInt(8);
    *extended = *extended;
    const bool extendedStable = extended->list().size() == 1 &&
        extended->list().GetItem(0)->AsInt()->value() == 8;
    PyDecRef(extended);
    return tupleStable && listStable && dictStable && extendedStable;
}

bool NotificationDecodeLifetimeRegression()
{
    // This malformed envelope used to release both the owning outer tuple
    // and its borrowed inner tuple, causing a double-free.
    PyTuple* invalid = new_tuple(new_tuple(new PyInt(0)), PyStatic.NewNone());
    EVENotificationStream broken;
    if (broken.Decode("macho.Notification", "OnInvalid", invalid) || invalid != nullptr)
        return false;

    PyTuple* inbound = new_tuple(
        new_tuple(
            new PyInt(0),
            new PySubStream(new_tuple(
                new PyInt(1), new_tuple(new PyInt(0), new_tuple(new PyInt(77)))
            ))
        ),
        PyStatic.NewNone()
    );
    EVENotificationStream notify;
    if (!notify.Decode("macho.Notification", "OnTest", inbound) || inbound != nullptr)
        return false;

    const bool decoded = notify.notifyType == "OnTest" &&
        notify.remoteObject == 1 && notify.args != nullptr &&
        notify.args->size() == 1 &&
        notify.args->GetItem(0)->AsInt()->value() == 77;
    EVENotificationStream* copy = notify.Clone();
    const bool cloned = copy != nullptr && copy->notifyType == "OnTest" &&
        copy->remoteObject == 1 && copy->args != notify.args &&
        copy->args->GetItem(0)->AsInt()->value() == 77;
    delete copy;
    return decoded && cloned;
}

bool CallPacketRoundTrip(bool channel, bool named, bool wrapped, bool malformed)
{
    PyCallStream outbound;
    outbound.method = "GetCharactersToSelect";
    outbound.arg_tuple = new_tuple(new PyInt(42));
    if (named) {
        outbound.arg_dict = new PyDict();
        outbound.arg_dict->SetItemString("test", new PyInt(7));
    }

    PyPacket packet;
    packet.type_string = "macho.CallReq";
    packet.type = CALL_REQ;
    packet.source.type = PyAddress::Client;
    packet.source.callID = 19;
    packet.dest.type = PyAddress::Node;
    packet.dest.service = "charUnboundMgr";
    packet.named_payload = new PyDict();
    packet.named_payload->SetItemString("trace", new PyInt(23));
    packet.contextKey = new PyString("test context");
    PyTuple* envelope = outbound.Encode();
    if (malformed) {
        envelope->SetItem(0, new_tuple(PyStatic.NewZero(), new PySubStream(new PyTuple(0))));
    }
    if (channel) {
        packet.payload = envelope;
    } else {
        PyRep* call = envelope->GetItem(0);
        PyIncRef(call);
        packet.payload = new_tuple(call);
        PyDecRef(envelope);
    }

    PyRep* encoded = packet.Encode();
    // Exercise wrapper ownership directly because some wrappers are decode-only.
    if (wrapped)
        encoded = new PyChecksumedStream(new PySubStream(encoded), 0);
    else {
        Buffer wire;
        const bool marshaled = MarshalDeflate(encoded, wire);
        PyDecRef(encoded);
        if (!marshaled)
            return false;
        encoded = InflateUnmarshal(wire);
    }

    PyCallStream inbound;
    {
        PyPacket decoded;
        if (!decoded.Decode(&encoded) || encoded != nullptr ||
            decoded.source.callID != 19 || decoded.dest.service != "charUnboundMgr" ||
            decoded.named_payload == nullptr || decoded.contextKey == nullptr ||
            PyRep::IntegerValue(decoded.named_payload->GetItemString("trace")) != 23 ||
            PyRep::StringContent(decoded.contextKey) != "test context")
            return false;
        if (inbound.Decode(decoded.type_string, decoded.payload) == malformed || decoded.payload != nullptr)
            return false;
    }

    if (malformed)
        return inbound.arg_tuple == nullptr && inbound.arg_dict == nullptr;
    return inbound.method == outbound.method && inbound.arg_tuple != nullptr &&
           inbound.arg_tuple->size() == 1 &&
           PyRep::IntegerValue(inbound.arg_tuple->GetItem(0)) == 42 &&
           (named ? inbound.arg_dict != nullptr &&
                        PyRep::IntegerValue(inbound.arg_dict->GetItemString("test")) == 7
                  : inbound.arg_dict == nullptr);
}
}

int marshal_EVEMarshalTest( int argc, char* argv[] )
{
    if (!WireObjectOwnershipRegression()) {
        ::puts("Python wire object clone/self-assignment lifetime regression.");
        return EXIT_FAILURE;
    }
    if (!NotificationDecodeLifetimeRegression()) {
        ::puts("Notification packet decode lifetime regression.");
        return EXIT_FAILURE;
    }
    if (!CallPacketRoundTrip(false, false, false, false) ||
        !CallPacketRoundTrip(true, true, false, false) ||
        !CallPacketRoundTrip(true, true, true, false) ||
        !CallPacketRoundTrip(false, false, false, true)) {
        ::puts("Call packet ownership or envelope regression.");
        return EXIT_FAILURE;
    }

    // The marshal wire stream has fields immediately following byte markers.
    // A 32-bit append therefore often lands at a non-aligned byte address.
    // Check that Buffer preserves the exact native representation without
    // making an invalid typed store (caught by UBSan on earlier builds).
    Buffer unaligned;
    const uint8 marker = 0x7e;
    const uint32 word = 0x12345678;
    unaligned.Append<uint8>(marker);
    unaligned.Append<uint32>(word);
    if (unaligned.size() != 1 + sizeof(word) ||
        unaligned[0] != marker ||
        std::memcmp(&unaligned[1], &word, sizeof(word)) != 0) {
        ::puts("Unaligned buffer append changed the marshal bytes.");
        return EXIT_FAILURE;
    }

    DBRowDescriptor *header = new DBRowDescriptor;
    // Fill header:
    header->AddColumn( "historyDate", DBTYPE_FILETIME );
    header->AddColumn( "lowPrice", DBTYPE_CY );
    header->AddColumn( "highPrice", DBTYPE_CY );
    header->AddColumn( "avgPrice", DBTYPE_CY );
    header->AddColumn( "volume", DBTYPE_I8 );
    header->AddColumn( "orders", DBTYPE_I4 );

    CRowSet* rs = new CRowSet( &header );

    PyPackedRow* row = rs->NewRow();
    row->SetField( "historyDate", new PyLong( Win32TimeNow() ) );
    row->SetField( "lowPrice", new PyLong( 18000 ) );
    row->SetField( "highPrice", new PyLong( 19000 ) );
    row->SetField( "avgPrice", new PyLong( 18400 ) );
    row->SetField( "volume", new PyLong( 5463586 ) );
    row->SetField( "orders", new PyInt( 254 ) );

    ::puts( "Marshaling..." );

    Buffer marshaled;
    bool res = MarshalDeflate( rs, marshaled );
    PyDecRef( rs );

    if( !res )
    {
        ::puts( "Failed to marshal Python object." );
        return EXIT_FAILURE;
    }

    ::puts( "Unmarshaling..." );

    PyRep* rep = InflateUnmarshal( marshaled );
    if( NULL == rep )
    {
        ::puts( "Failed to unmarshal Python object." );
        return EXIT_FAILURE;
    }

    // UnmarshalStream creates generic PyObjectEx containers. Packed rows
    // inside them must have a *real* DBRowDescriptor reconstructed from the
    // wire header, not an unsafe cast of a PyObjectEx to DBRowDescriptor.
    if (!rep->IsObjectEx() || rep->AsObjectEx()->list().size() != 1 ||
        !rep->AsObjectEx()->list().GetItem(0)->IsPackedRow()) {
        ::puts( "Unmarshaled result does not contain the expected packed row." );
        PyDecRef(rep);
        return EXIT_FAILURE;
    }
    PyPackedRow* decoded = rep->AsObjectEx()->list().GetItem(0)->AsPackedRow();
    if (decoded->header()->ColumnCount() != 6 ||
        decoded->header()->FindColumn("lowPrice") != 1 ||
        decoded->header()->FindColumn("orders") != 5 ||
        !decoded->GetField(1)->IsLong() ||
        decoded->GetField(1)->AsLong()->value() != 18000 ||
        !decoded->GetField(4)->IsLong() ||
        decoded->GetField(4)->AsLong()->value() != 5463586 ||
        !decoded->GetField(5)->IsInt() ||
        decoded->GetField(5)->AsInt()->value() != 254) {
        ::puts( "Unmarshaled packed row descriptor or values differ from source." );
        PyDecRef(rep);
        return EXIT_FAILURE;
    }

    ::puts( "Final:" );
    rep->Dump( stdout, "    " );
    PyDecRef( rep );

    return EXIT_SUCCESS;
}
