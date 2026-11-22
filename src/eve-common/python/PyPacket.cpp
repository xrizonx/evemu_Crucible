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
    Author:     Zhur
    Rewrite:    Allan
*/

/** @todo  update this to unique_ptr/shared_ptr if possible  shared_ptr<T> p(new Y); or make_unique<>() */

#include "eve-common.h"

#include "python/PyPacket.h"
#include "python/PyVisitor.h"
#include "python/PyRep.h"
#include "python/PyDumpVisitor.h"

#include <memory>
#include <utility>

namespace {
struct ReleasePyRep {
    void operator()(PyRep* rep) const { PySafeDecRef(rep); }
};
using OwnedPyRep = std::unique_ptr<PyRep, ReleasePyRep>;
}

const char* MACHONETMSG_TYPE_NAMES[MACHONETMSG_TYPE_COUNT] =
{
    "AUTHENTICATION_REQ",
    "AUTHENTICATION_RSP",
    "IDENTIFICATION_REQ",
    "IDENTIFICATION_RSP",
    "U4",
    "U5",
    "CALL_REQ",
    "CALL_RSP",
    "TRANSPORTCLOSED",
    "U9",
    "RESOLVE_REQ",
    "RESOLVE_RSP",
    "NOTIFICATION",
    "U13",
    "U14",
    "ERRORRESPONSE",
    "SESSIONCHANGENOTIFICATION",
    "U17",
    "SESSIONINITIALSTATENOTIFICATION",
    "U19",
    "PING_REQ",
    "PING_RSP"
};

PyPacket::PyPacket()
: type_string("none"),
type(__Fake_Invalid_Type),
userid(0),
payload(nullptr),
named_payload(nullptr),
contextKey(nullptr)
{
}

PyPacket::~PyPacket()
{
    PySafeDecRef(payload);
    PySafeDecRef(named_payload);
    PySafeDecRef(contextKey);
}

PyPacket *PyPacket::Clone() const
{
    PyPacket *res = new PyPacket();
        res->type_string = type_string;
        res->type = type;
        res->source = source;
        res->dest = dest;
        res->userid = userid;
        if (payload != nullptr)
            res->payload = payload->Clone()->AsTuple();
    if (named_payload == nullptr)
        res->named_payload = nullptr;
    else
        res->named_payload = named_payload->Clone()->AsDict();
    if (contextKey != nullptr)
        res->contextKey = contextKey->Clone();
    return res;
}

void PyPacket::Dump(LogType ltype, PyVisitor& dumper)
{
    _log(ltype, "Packet:");
    _log(ltype, "  Type: %s", type_string.c_str());
    _log(ltype, "  Command: %s (%d)", MACHONETMSG_TYPE_NAMES[type], type);
    _log(ltype, "  Source:");
    source.Dump(ltype, "    ");
    _log(ltype, "  Dest:");
    dest.Dump(ltype, "    ");
    _log(ltype, "  User ID: %u", userid);
    _log(ltype, "  Payload:");
    payload->visit( dumper );
    if (named_payload == nullptr) {
        _log(ltype, "  Named Payload: None (null)");
    } else {
        _log(ltype, "  Named Payload:");
        named_payload->visit( dumper );
    }
}

bool PyPacket::Decode(PyRep **in_packet)
{
    OwnedPyRep root(std::exchange(*in_packet, nullptr));
    PyRep* packet = root.get();

    PySafeDecRef(payload);
    PySafeDecRef(named_payload);
    payload = nullptr;
    named_payload = nullptr;
    PySafeDecRef(contextKey);
    contextKey = nullptr;

    if (packet == nullptr) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - packet is null.");
        return false;
    }

    if (packet->IsChecksumedStream()) {
        //TODO: check cs->checksum
        packet = packet->AsChecksumedStream()->stream();
    }
    //Dragon nuance... it gets wrapped again
    if (packet != nullptr && packet->IsSubStream()) {
        PySubStream* ss = packet->AsSubStream();
        ss->DecodeData();
        if (ss->decoded() == nullptr) {
            codelog(NET__PACKET_ERROR, "PyPacket::Decode() - unable to decode initial packet substream.");
            return false;
        }

        packet = ss->decoded();
    }

    if (packet == nullptr || !packet->IsObject()) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - packet body is not PyObject: %s", packet == nullptr ? "null" : packet->TypeString());
        return false;
    }

    type_string = packet->AsObject()->type()->content();
    if (packet->AsObject()->arguments() == nullptr || !packet->AsObject()->arguments()->IsTuple()) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - packet body does not contain a tuple");
        return false;
    }

    PyTuple *tuple = packet->AsObject()->arguments()->AsTuple();
    if (tuple == nullptr) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - tuple is null.");
        return false;
    }

    if (tuple->items.size() != 7) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - packet body does not contain a tuple of length 7 (is %zu)", tuple->items.size());
        return false;
    }

    if (tuple->items[0] == nullptr || !tuple->items[0]->IsInt()) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - First main tuple element is not an integer");
        return false;
    }

    switch(PyRep::IntegerValue(tuple->items[0])) {
        case AUTHENTICATION_REQ:
        case AUTHENTICATION_RSP:
        case IDENTIFICATION_REQ:
        case IDENTIFICATION_RSP:
        case CALL_REQ:
        case CALL_RSP:
        case TRANSPORTCLOSED:
        case RESOLVE_REQ:
        case RESOLVE_RSP:
        case NOTIFICATION:
        case ERRORRESPONSE:
        case SESSIONCHANGENOTIFICATION:
        case SESSIONINITIALSTATENOTIFICATION:
        case PING_REQ:
        case PING_RSP: {
            type = (MACHONETMSG_TYPE) PyRep::IntegerValue(tuple->items[0]);
        } break;
        default: {
            codelog(NET__PACKET_ERROR, "PyPacket::Decode() - Unknown message type %lld", static_cast<long long>(PyRep::IntegerValue(tuple->items[0])));
            return false;
        } break;
    }

    PyRep* sourceAddress = tuple->items[1];
    PySafeIncRef(sourceAddress);
    if (!source.Decode(sourceAddress))
        return false;

    PyRep* destAddress = tuple->items[2];
    PySafeIncRef(destAddress);
    if (!dest.Decode(destAddress))
        return false;

    userid = PyRep::IntegerValue(tuple->items[3]);

    if (tuple->items[4] == nullptr || !tuple->items[4]->IsTuple()) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - Fifth main tuple element is not a tuple");
        return false;
    }

    PyRep* options = tuple->items[5];
    if (options == nullptr || !(options->IsNone() || options->IsDict())) {
        codelog(NET__PACKET_ERROR, "PyPacket::Decode() - Sixth main tuple element is neither dict nor none.");
        return false;
    }

    payload = tuple->items[4]->AsTuple();
    PyIncRef(payload);
    if (options->IsDict()) {
        named_payload = options->AsDict();
        PyIncRef(named_payload);
    }
    if (tuple->items[6] != nullptr && !tuple->items[6]->IsNone()) {
        contextKey = tuple->items[6];
        PyIncRef(contextKey);
    }
    return true;
}

PyRep *PyPacket::Encode() {
    PyTuple *arg_tuple = new PyTuple(7);
    //command
    arg_tuple->items[0] = new PyInt(type);
    //source
    arg_tuple->items[1] = source.Encode();
    //dest
    arg_tuple->items[2] = dest.Encode();
    //userid
    if (userid == 0)
        arg_tuple->items[3] = PyStatic.NewNone();
    else
        arg_tuple->items[3] = new PyInt(userid);

    //payload
    PySafeIncRef(payload);
    arg_tuple->items[4] = payload;

    //named arguments (OID+ or sn)
    if (named_payload == nullptr) {
        arg_tuple->items[5] = PyStatic.NewNone();
    } else {
        PyIncRef(named_payload);
        arg_tuple->items[5] = named_payload;
    }

    // contextKey, gives the client extra information on what exactly is going on
    // this is PyNone almost 100% of the time and seems to be used in the node<->proxy communication
    if (contextKey == nullptr) {
        arg_tuple->items[6] = PyStatic.NewNone();
    } else {
        PyIncRef(contextKey);
        arg_tuple->items[6] = contextKey;
    }

    return new PyObject( type_string.c_str(), arg_tuple );
}

PyAddress::PyAddress()
: type(Invalid),
objectID(0),
callID(0),
service("")
{
}

void PyAddress::Dump(FILE *into, const char *pfx) const {
    switch(type) {
        case Any:
            fprintf(into, "%sAny: service='%s' callID=%lld", pfx, service.c_str(), static_cast<long long>(callID));
        break;
        case Node:
            fprintf(into, "%sNode: nodeID=%lld service='%s' callID=%lld", pfx, static_cast<long long>(objectID), service.c_str(), static_cast<long long>(callID));
        break;
        case Client:
            fprintf(into, "%sClient: clientID=%lld service='%s' callID=%lld", pfx, static_cast<long long>(objectID), service.c_str(), static_cast<long long>(callID));
        break;
        case Broadcast:
            fprintf(into, "%sBroadcast: broadcastID='%s' narrowcast=(not implemented) idtype='%s'", pfx, service.c_str(), bcast_idtype.c_str());
        break;
        case Invalid:
        break;
    //no default on purpose
    }
}

void PyAddress::Dump(LogType ltype, const char *pfx) const {
    switch(type) {
        case Any:
            _log(ltype, "%sAny: service='%s' callID=%lli", pfx, service.c_str(), callID);
        break;
        case Node:
            _log(ltype, "%sNode: nodeID=%lli service='%s' callID=%lli", pfx, objectID, service.c_str(), callID);
        break;
        case Client:
            _log(ltype, "%sClient: clientID=%lli callID=%lli service='%s'", pfx, objectID, callID, service.c_str());
        break;
        case Broadcast:
            _log(ltype, "%sBroadcast: broadcastID='%s' narrowcast=(not implemented) idtype='%s'", pfx, service.c_str(), bcast_idtype.c_str());
        break;
        case Invalid:
        break;
    //no default on purpose
    }
}

void PyAddress::operator=(const PyAddress &right) {
    type = right.type;
    objectID = right.objectID;
    callID = right.callID;
    service = right.service;
    bcast_idtype = right.bcast_idtype;
}

bool PyAddress::Decode(PyRep *&in_object) {
    OwnedPyRep base(std::exchange(in_object, nullptr));

    if (base == nullptr) {
        codelog(NET__PACKET_ERROR, "PyAddress::Decode() - base is null.");
        return false;
    }

    if (!base->IsObject()) {
        codelog(NET__PACKET_ERROR, "Invalid element type, expected object but got %s", base->TypeString());
        return false;
    }

    PyRep* arguments = base->AsObject()->arguments();
    if (arguments == nullptr || !arguments->IsTuple()) {
        codelog(NET__PACKET_ERROR, "PyAddress::Decode() - address arguments must be a tuple.");
        return false;
    }

    PyTuple* tuple = arguments->AsTuple();
    if (tuple->items.size() < 3) {
        codelog(NET__PACKET_ERROR, "Not enough elements in address tuple: %zu", tuple->items.size());
        tuple->Dump(NET__PACKET_ERROR, "  ");
        return false;
    }

    //decode the address type.
    if (tuple->items[0] == nullptr || !tuple->items[0]->IsInt()) {
        codelog(NET__PACKET_ERROR, "Wrong type on address element (0)");
        return false;
    }

    switch(PyRep::IntegerValue(tuple->items[0])) {
        case Any: {
            if (tuple->items.size() != 3) {
                codelog(NET__PACKET_ERROR, "Invalid number of elements in Any address tuple: %zu", tuple->items.size());
                return false;
            }
            type = Any;

            if (!_DecodeService(tuple->items[1])
            or !_DecodeCallID(tuple->items[2])) {
                return false;
            }
        }  break;
        case Node: {
            if (tuple->items.size() != 4) {
                codelog(NET__PACKET_ERROR, "Invalid number of elements in Node address tuple: %zu", tuple->items.size());
                return false;
            }
            type = Node;

            if (!_DecodeObjectID(tuple->items[1])
            or !_DecodeService(tuple->items[2])
            or !_DecodeCallID(tuple->items[3])) {
                return false;
            }
        }  break;
        case Client: {
            if (tuple->items.size() != 4) {
                codelog(NET__PACKET_ERROR, "Invalid number of elements in Client address tuple: %zu", tuple->items.size());
                return false;
            }
            type = Client;

            if (!_DecodeObjectID(tuple->items[1])
            or !_DecodeCallID(tuple->items[2])
            or !_DecodeService(tuple->items[3])) {
                return false;
            }
        }  break;
        case Broadcast: {
            if (tuple->items.size() != 4) {
                codelog(NET__PACKET_ERROR, "Invalid number of elements in Broadcast address tuple: %zu", tuple->items.size());
                return false;
            }
            type = Broadcast;

            if (tuple->items[1] == nullptr || !tuple->items[1]->IsString()) {
                codelog(NET__PACKET_ERROR, "Invalid type %s for brodcastID", tuple->items[1] == nullptr ? "null" : tuple->items[1]->TypeString());
                return false;
            }
            if (tuple->items[3] == nullptr || !tuple->items[3]->IsString()) {
                codelog(NET__PACKET_ERROR, "Invalid type %s for idtype", tuple->items[3] == nullptr ? "null" : tuple->items[3]->TypeString());
                return false;
            }

            service = PyRep::StringContent(tuple->items[1]);       //assign op
            bcast_idtype = PyRep::StringContent(tuple->items[3]);  //assign op

            //items[2] is either a list or a tuple.
            /*
            //PyList *nclist = (PyList *) tuple->items[2];
            if (!nclist->items.empty()) {
                printf("Not decoding narrowcast list:");
                nclist->Dump(NET__PACKET_ERROR, "     ");
            }*/
        }   break;
        default: {
            codelog(NET__PACKET_ERROR, "Unknown address type: %lld", static_cast<long long>(PyRep::IntegerValue(tuple->items[0])));
            return false;
        }
    }

    return true;
}

PyRep *PyAddress::Encode() {
    PyTuple *t(nullptr);
    switch(type) {
        case Any: {
            t = new PyTuple(3);
            t->items[0] = new PyInt((int)type);

            if (service.empty())
                t->items[1] = PyStatic.NewNone();
            else
                t->items[1] = new PyString(service.c_str());

            if (objectID == 0)
                t->items[2] = PyStatic.NewNone();
            else
                t->items[2] = new PyLong(objectID);
        } break;
        case Node: {
            t = new PyTuple(4);
            t->items[0] = new PyInt((int)type);
            t->items[1] = new PyLong(objectID);

            if (service.empty())
                t->items[2] = PyStatic.NewNone();
            else
                t->items[2] = new PyString(service.c_str());

            if (callID == 0)
                t->items[3] = PyStatic.NewNone();
            else
                t->items[3] = new PyLong(callID);
        } break;
        case Client: {
            t = new PyTuple(4);
            t->items[0] = new PyInt((int)type);
            t->items[1] = new PyLong(objectID);

            if (callID == 0)
                t->items[2] = PyStatic.NewNone();
            else
                t->items[2] = new PyLong(callID);

            if (service.empty())
                t->items[3] = PyStatic.NewNone();
            else
                t->items[3] = new PyString(service.c_str());
        } break;
        case Broadcast: {
            t = new PyTuple(4);
            t->items[0] = new PyInt((int)type);
            //broadcastID
            if (service.empty())
                t->items[1] = PyStatic.NewNone();
            else
                t->items[1] = new PyString(service.c_str());
            //narrowcast
            t->items[2] = new PyList(); // LSC uses tuple here, others None() or empty List()
            //typeID
            t->items[3] = new PyString(bcast_idtype.c_str());
        } break;
        case Invalid:
        default: {
            //this still needs to be something which will not crash us.
            t = new_tuple(PyStatic.NewNone());
        } break;
    }

    return new PyObject( "macho.MachoAddress", t );
}

bool PyAddress::_DecodeService(PyRep *rep) {
    service = PyRep::StringContent(rep);
    return true;
}

bool PyAddress::_DecodeCallID(PyRep *rep) {
    callID = PyRep::IntegerValue(rep);
    return true;
}

bool PyAddress::_DecodeObjectID(PyRep *rep) {
    objectID = PyRep::IntegerValue(rep);
    return true;
}


PyCallStream::PyCallStream()
: remoteObject(0),
  method(""),
  arg_tuple(nullptr),
  arg_dict(nullptr)
{
}

PyCallStream::~PyCallStream() {
    PySafeDecRef(arg_tuple);
    PySafeDecRef(arg_dict);
}

PyCallStream *PyCallStream::Clone() const {
    PyCallStream *res = new PyCallStream();
    res->remoteObject = remoteObject;
    res->remoteObjectStr = remoteObjectStr;
    res->method = method;
    res->arg_tuple = arg_tuple->Clone()->AsTuple();
    if (arg_dict == nullptr)
        res->arg_dict = nullptr;
    else
        res->arg_dict = arg_dict->Clone()->AsDict();

    return res;
}

void PyCallStream::Dump(LogType type, PyVisitor& dumper)
{
    _log(type, "Call Stream:");
    if (remoteObject == 0)
        _log(type, "  Remote Object: '%s'", remoteObjectStr.c_str());
    else
        _log(type, "  Remote Object: %u", remoteObject);
    _log(type, "  Method: %s", method.c_str());
    _log(type, "  Arguments:");
    arg_tuple->visit( dumper );
    if (arg_dict == nullptr) {
        _log(type, "  Named Arguments: None");
    } else {
        _log(type, "  Named Arguments:");
        arg_dict->visit( dumper );
    }
}

bool PyCallStream::Decode(const std::string &type, PyTuple *&in_payload) {
    OwnedPyRep root(std::exchange(in_payload, nullptr));
    PySafeDecRef(arg_tuple);
    PySafeDecRef(arg_dict);
    arg_tuple = nullptr;
    arg_dict = nullptr;

    if (type != "macho.CallReq" || root == nullptr) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - invalid call packet type '%s' or missing payload", type.c_str());
        return false;
    }

    const auto& payload = root->AsTuple()->items;
    // The optional second element is the channel metadata.
    if (payload.size() != 1 && payload.size() != 2) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - invalid tuple length %zu", payload.size());
        return false;
    }
    if (payload[0] == nullptr || !payload[0]->IsTuple()) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - expected an inner call tuple");
        return false;
    }

    const auto& envelope = payload[0]->AsTuple()->items;
    if (envelope.size() != 2 || envelope[1] == nullptr || !envelope[1]->IsSubStream()) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - expected a call substream");
        return false;
    }

    PySubStream* stream = envelope[1]->AsSubStream();
    stream->DecodeData();
    PyRep* decoded = stream->decoded();
    if (decoded == nullptr || !decoded->IsTuple() || decoded->AsTuple()->size() != 4) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - expected four fields in the decoded call");
        return false;
    }

    const auto& fields = decoded->AsTuple()->items;
    if (fields[0] == nullptr || !(fields[0]->IsInt() || fields[0]->IsString()) ||
        fields[1] == nullptr || !fields[1]->IsString() ||
        fields[2] == nullptr || !fields[2]->IsTuple() ||
        fields[3] == nullptr || !(fields[3]->IsNone() || fields[3]->IsDict())) {
        codelog(NET__PACKET_ERROR, "PyCallStream::Decode() - invalid call object, method, or arguments");
        return false;
    }

    remoteObject = 0;
    remoteObjectStr.clear();
    if (fields[0]->IsInt())
        remoteObject = PyRep::IntegerValue(fields[0]);
    else
        remoteObjectStr = PyRep::StringContent(fields[0]);
    method = PyRep::StringContent(fields[1]);
    arg_tuple = fields[2]->AsTuple();
    PyIncRef(arg_tuple);
    if (fields[3]->IsDict()) {
        arg_dict = fields[3]->AsDict();
        PyIncRef(arg_dict);
    }
    return true;
}

PyTuple *PyCallStream::Encode() {
    PyTuple *res_tuple = new PyTuple(4);

    //remoteObject
    if (remoteObject == 0)
        res_tuple->items[0] = new PyString(remoteObjectStr.c_str());
    else
        res_tuple->items[0] = new PyInt(remoteObject);

    //method name
    res_tuple->items[1] = new PyString(method.c_str());

    //args
    PySafeIncRef(arg_tuple);
    res_tuple->items[2] = arg_tuple;

    //options
    if (arg_dict == nullptr)
        res_tuple->items[3] = PyStatic.NewNone();
    else {
        PyIncRef(arg_dict);
        res_tuple->items[3] = arg_dict;
    }

    //now that we have the main arg tuple, build the unknown stuff around it...
    PyTuple *it2 = new PyTuple(2);
        it2->items[0] = new PyInt(remoteObject==0?1:0); /* some sort of flag, "process here or call UP"....*/
        it2->items[1] = new PySubStream(res_tuple);
    PyTuple *it1 = new PyTuple(2);
        it1->items[0] = it2;
        it1->items[1] = PyStatic.NewNone();    //this is the "channel" dict if populated.
    return it1;
}

EVENotificationStream::EVENotificationStream()
: notifyType("NO TYPE SET"),
  remoteObject(0),
  args(nullptr)
{
}

EVENotificationStream::~EVENotificationStream() {
    PySafeDecRef(args);
}

EVENotificationStream *EVENotificationStream::Clone() const {
    EVENotificationStream *res = new EVENotificationStream();
    res->args = args->Clone()->AsTuple();
    return res;
}

void EVENotificationStream::Dump(LogType type, PyVisitor& dumper)
{
    _log(type, "Notification: %s", notifyType.c_str());
    if (remoteObject == 0)
        _log(type, "  Remote Object: %s", remoteObjectStr.c_str());
    else
        _log(type, "  Remote Object: %u", remoteObject);

    _log(type, "  Arguments:");
    args->visit( dumper );
}

bool EVENotificationStream::Decode(const std::string &pkt_type, const std::string &notify_type, PyTuple *&in_payload) {
    // The incoming packet owns all nested PyReps. A nested tuple/substream
    // is borrowed while decoding; never DecRef it separately. Previously
    // both the success path and many error paths released the same objects
    // repeatedly, causing fatal IncRef/DecRef use-after-free at login.
    OwnedPyRep root(std::exchange(in_payload, nullptr));
    PySafeDecRef(args);
    args = nullptr;

    if (root == nullptr || pkt_type != "macho.Notification") {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid packet type or missing payload.");
        return false;
    }

    const auto& envelope = root->AsTuple()->items;
    if (envelope.size() != 2 || envelope[0] == nullptr || !envelope[0]->IsTuple()) {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid outer envelope.");
        return false;
    }

    const auto& envelope2 = envelope[0]->AsTuple()->items;
    if (envelope2.size() != 2 || envelope2[1] == nullptr || !envelope2[1]->IsSubStream()) {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - missing notification substream.");
        return false;
    }

    PySubStream* stream = envelope2[1]->AsSubStream();
    stream->DecodeData();
    PyRep* decoded = stream->decoded();
    if (decoded == nullptr || !decoded->IsTuple()) {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid notification body.");
        return false;
    }

    const auto& objectCall = decoded->AsTuple()->items;
    if (objectCall.size() != 2 || objectCall[0] == nullptr ||
        objectCall[1] == nullptr || !objectCall[1]->IsTuple()) {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid object call.");
        return false;
    }

    uint32 nextRemoteObject = 0;
    std::string nextRemoteObjectStr;
    if (objectCall[0]->IsInt()) {
        nextRemoteObject = PyRep::IntegerValue(objectCall[0]);
    } else if (objectCall[0]->IsString()) {
        nextRemoteObjectStr = PyRep::StringContent(objectCall[0]);
    } else {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid remote object type.");
        return false;
    }

    const auto& body = objectCall[1]->AsTuple()->items;
    if (body.size() != 2 || body[0] == nullptr || !body[0]->IsInt() ||
        body[1] == nullptr || !body[1]->IsTuple()) {
        codelog(NET__PACKET_ERROR, "EVENotificationStream::Decode() - invalid notification arguments.");
        return false;
    }

    // Retain the argument tuple before root's destructor releases its owners.
    PyTuple* nextArgs = body[1]->AsTuple();
    PyIncRef(nextArgs);
    args = nextArgs;
    remoteObject = nextRemoteObject;
    remoteObjectStr = std::move(nextRemoteObjectStr);
    notifyType = notify_type;
    return true;
}

PyTuple *EVENotificationStream::Encode() {
    PyTuple *t4 = new PyTuple(2);
        t4->SetItem(0, PyStatic.NewOne());
        t4->SetItem(1, args);       // no need to clone here.  set actual rep in item, and it will be cleaned up by d'tor later
    PyTuple *t3 = new PyTuple(2);
        t3->SetItem(0, new PyInt(0));
        t3->SetItem(1, t4);
    PyTuple *t2 = new PyTuple(2);
        t2->SetItem(0, new PyInt(0));
        t2->SetItem(1, new PySubStream(t3));
    PyTuple *t1 = new PyTuple(1);
        t1->SetItem(0, t2);
    return t1;
}
