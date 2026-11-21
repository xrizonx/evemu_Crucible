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
    Updates:    Allan
*/

#include "eve-server.h"


#include "character/SkillMgrService.h"

SkillMgrService::SkillMgrService(EVEServiceManager& mgr)
: BindableService("skillMgr", mgr, eAccessLevel_Character)
{
}

BoundDispatcher* SkillMgrService::BindObject (Client *client, PyRep* bindParameters) {
    _log(CLIENT__MESSAGE, "SkillMgrService bind request for:");

    return new SkillMgrBound(this->GetServiceManager(), *this, m_db);
}

void SkillMgrService::BoundReleased (SkillMgrBound* bound) {

}

SkillMgrBound::SkillMgrBound(EVEServiceManager &mgr, SkillMgrService& parent, CharacterDB &db) :
    EVEBoundObject(mgr, parent),
    m_db(db)
{
    this->Add("InjectSkillIntoBrain", &SkillMgrBound::InjectSkillIntoBrain);
    this->Add("GetSkillQueueAndFreePoints", &SkillMgrBound::GetSkillQueueAndFreePoints);
    this->Add("SaveSkillQueue", &SkillMgrBound::SaveSkillQueue);
    this->Add("AddToEndOfSkillQueue", &SkillMgrBound::AddToEndOfSkillQueue);
    this->Add("CharStartTrainingSkill", &SkillMgrBound::CharStartTrainingSkill);
    this->Add("CharStartTrainingSkillByTypeID", &SkillMgrBound::CharStartTrainingSkillByTypeID);
    this->Add("CharStopTrainingSkill", &SkillMgrBound::CharStopTrainingSkill);
    this->Add("GetEndOfTraining", &SkillMgrBound::GetEndOfTraining);
    this->Add("GetSkillHistory", &SkillMgrBound::GetSkillHistory);
    this->Add("CharAddImplant", &SkillMgrBound::CharAddImplant);
    this->Add("RemoveImplantFromCharacter", &SkillMgrBound::RemoveImplantFromCharacter);
    this->Add("GetRespecInfo", &SkillMgrBound::GetRespecInfo);
    this->Add("RespecCharacter", &SkillMgrBound::RespecCharacter);
    this->Add("GetCharacterAttributeModifiers", &SkillMgrBound::GetCharacterAttributeModifiers);
}

PyResult SkillMgrBound::GetRespecInfo(PyCallArgs& call) {
    return m_db.GetRespecInfo(call.client->GetCharacterID());
}

PyResult SkillMgrBound::GetSkillQueueAndFreePoints(PyCallArgs &call) {
    return call.client->GetChar()->SendSkillQueue();
}

PyResult SkillMgrBound::GetEndOfTraining(PyCallArgs &call) {
    return new PyLong( call.client->GetChar()->GetEndOfTraining() );
}

PyResult SkillMgrBound::GetSkillHistory(PyCallArgs& call) {
    return call.client->GetChar()->GetSkillHistory();
}

PyResult SkillMgrBound::CharStopTrainingSkill(PyCallArgs &call) {
    // called when pausing skill queue
    call.client->GetChar()->PauseSkillQueue();
    // returns nothing
    return nullptr;
}

PyResult SkillMgrBound::CharStartTrainingSkill(PyCallArgs& call, PyInt* itemID, PyInt* locationID) {
    // sm.GetService('godma').GetSkillHandler().CharStartTrainingSkill(skillX.itemID, skillX.locationID)
    _log(SKILL__WARNING, "Called CharStartTrainingSkill for itemID %i in location %i", itemID->value(), locationID->value());
    return nullptr;
}

PyResult SkillMgrBound::AddToEndOfSkillQueue(PyCallArgs &call, PyInt* skillID, PyInt* nextLevel) {
    //  sm.StartService('godma').GetSkillHandler().AddToEndOfSkillQueue(skillID, nextLevel)
    CharacterRef cRef(call.client->GetChar());
    cRef->AddToSkillQueue(skillID->value(), nextLevel->value());
    cRef->UpdateSkillQueueEndTime();
    return nullptr;
}

PyResult SkillMgrBound::InjectSkillIntoBrain(PyCallArgs &call, PyList* skillItemIDs, PyInt* stationID)
{
    std::vector<int32> skillItemIDsList;

    PyList::const_iterator list_2_cur = skillItemIDs->begin();
    for (size_t list_2_index(0); list_2_cur != skillItemIDs->end(); ++list_2_cur, ++list_2_index) {
        if (!(*list_2_cur)->IsInt()) {
            _log(XMLP__DECODE_ERROR, "Decode Call_InjectSkillIntoBrain failed: Element %u in list list_2 is not an integer: %s", list_2_index, (*list_2_cur)->TypeString());
            return nullptr;
        }

        const PyInt* t = (*list_2_cur)->AsInt();
        skillItemIDsList.push_back(t->value());
    }

    // make a list of skills successfully injected to display after injection
    // name, ret value  where 1=success, 2=prereqs, 3=already known, 4=split fail, 5=load fail
    std::map<std::string, uint8> skills;
    SkillRef skill(nullptr);
    CharacterRef cRef(call.client->GetChar());
    for (auto cur : skillItemIDsList)  {
        skill = sItemFactory.GetSkillRef(cur);
        if (skill.get() == nullptr) {
            _log( ITEM__ERROR, "%s: failed to load skill %u for injection.", call.client->GetName(), cur);
            std::string str = "Invalid Name #";
            str += std::to_string(cur);
            skills.emplace(str, 5);
            continue;
        }

        skills.emplace(skill->itemName(), cRef->InjectSkillIntoBrain(skill));
    }

    // build and populate status reply
    if (skills.empty())
        return nullptr;

    if (skills.size() == 1) {
        std::string status;
        switch (skills.begin()->second) {
            //1=success, 2=prereqs, 3=already known, 4=split fail, 5=load fail
            case 1: status = "<color=green>Success.</color>"; break;
            case 2: status = "<color=red>Failed:</color> <color=yellow>Prerequisites incomplete.</color>"; break;
            case 3: status = "<color=red>Failed:</color> <color=cyan>Skill already known.</color>"; break;
            case 4: status = "<color=red>Failed:</color> <color=red>Stack split failure.</color>"; break;
            case 5: status = "<color=red>Failed:</color> <color=maroon>Skill loading failure.</color>"; break;
            default: status = "<color=red>Failed:</color> <color=red>Unknown Error.</color>"; break;
        }
        call.client->SendInfoModalMsg("Injection of %s:  %s", skills.begin()->first.c_str(), status.c_str());
    } else {
        std::string status;
        std::ostringstream str;
        str.clear();
        str << "The Injection of %u skills for %s has resulted in the following outcome.<br><br>"; //40

        for (auto cur : skills) {
            switch (cur.second) {
                //1=success, 2=prereqs, 3=already known, 4=split fail, 5=load fail
                case 1: status = "<color=green>Success.</color>"; break;
                case 2: status = "<color=red>Failed:</color> <color=yellow>Prerequisites incomplete.</color>"; break;
                case 3: status = "<color=red>Failed:</color> <color=cyan>Skill already known.</color>"; break;
                case 4: status = "<color=red>Failed:</color> <color=red>Stack split failure.</color>"; break;
                case 5: status = "<color=red>Failed:</color> <color=maroon>Skill loading failure.</color>"; break;
                default: status = "<color=red>Failed:</color> <color=red>Unknown Error.</color>"; break;
            }
            str << cur.first << " - " << status << "<br>"; //40 for name, 80 for status (120)
        }

        int size = skills.size() * 120;
        size += 100;    // for header, including char name
        char* reply = Memory::Allocator::NewArray<char>(&sAllocators.tickAllocator, size);
        snprintf(reply, size, str.str().c_str(), skills.size(), call.client->GetName());

        call.client->SendInfoModalMsg(reply);
    }

    PyTuple* tmp = new PyTuple(1);
    tmp->SetItem(0, new PyString("OnSkillInjected"));
    call.client->QueueDestinyEvent(&tmp);
    return nullptr;
}

PyResult SkillMgrBound::SaveSkillQueue(PyCallArgs &call, PyList* skillQueue) {
    // xml decode will now check for and fix level being a float instead of int and leading to client freakout
    CharacterRef cRef(call.client->GetChar());
    _log(SKILL__QUEUE, "%s(%u) calling SaveSkillQueue()", cRef->name(), cRef->itemID());
    cRef->ClearSkillQueue(true);
    SkillQueue_Element el;
    PyList::const_iterator itr = skillQueue->begin(), end = skillQueue->end();
    for (; itr != end; ++itr) {
        if (!el.Decode(*itr))         {
            _log(SERVICE__ERROR, "%s: Failed to decode element of SkillQueue (%u). Skipping.", call.client->GetName(), PyRep::IntegerValueU32(*itr));
            continue;
        }
        cRef->AddToSkillQueue( el.typeID, el.level );
    }

    cRef->UpdateSkillQueueEndTime();
    PyTuple* tmp = new PyTuple(1);
        tmp->SetItem(0, new PyString("OnSkillTrainingSaved"));
    call.client->QueueDestinyEvent(&tmp);
    return nullptr;
}

PyResult SkillMgrBound::CharStartTrainingSkillByTypeID(PyCallArgs& call, PyInt* skillTypeID)
{
    // called when skill queue empty or paused
    // sends skill typeID to start training
    call.client->GetChar()->LoadPausedSkillQueue(skillTypeID->value());
    return nullptr;
}

PyResult SkillMgrBound::RespecCharacter(PyCallArgs &call, PyInt* charisma, PyInt* intelligence, PyInt* memory, PyInt* perception, PyInt* willpower)
{
    CharacterRef cRef(call.client->GetChar());
    if (cRef->GetSkillInTraining() != nullptr)
        throw UserError ("RespecSkillInTraining");

    // return early if this is an illegal call
    if (!m_db.ReportRespec(call.client->GetCharacterID()))
        return nullptr;
    uint8 multiplier(sConfig.character.statMultiplier);
    cRef->SetAttribute(AttrCharisma, charisma->value() * multiplier);
    cRef->SetAttribute(AttrIntelligence, intelligence->value() * multiplier);
    cRef->SetAttribute(AttrMemory, memory->value() * multiplier);
    cRef->SetAttribute(AttrPerception, perception->value() * multiplier);
    cRef->SetAttribute(AttrWillpower, willpower->value() * multiplier);
    cRef->SaveAttributes();

    // no return value
    return nullptr;
}

PyResult SkillMgrBound::GetCharacterAttributeModifiers(PyCallArgs &call, PyInt* attr)
{
    const auto attributeID = attr->value();
    if (attributeID <= 0 || attributeID > UINT16_MAX)
        return new PyList();
    return call.client->GetChar()->GetImplantAttributeModifiers(static_cast<uint16>(attributeID));
}

PyResult SkillMgrBound::CharAddImplant(PyCallArgs& call, PyInt* itemID)
{
    if (itemID->value() <= 0)
        throw CustomError("The implant could not be found.");

    auto* client = call.client;
    const uint32 characterID = client->GetCharacterID();
    InventoryItemRef implant = sItemFactory.GetItemRef(itemID->value());
    if (implant.get() == nullptr || implant->ownerID() != characterID)
        throw CustomError("The implant must be in your personal hangar or active ship cargo.");

    const bool inHangar = client->IsDocked() && implant->flag() == flagHangar &&
        implant->locationID() == static_cast<uint32>(client->GetStationID());
    const bool inCargo = implant->flag() == flagCargoHold && implant->locationID() == client->GetShipID();
    if ((!inHangar && !inCargo) || (!implant->isSingleton() && implant->quantity() < 1))
        throw CustomError("The implant must be in your personal hangar or active ship cargo.");

    client->GetChar()->InstallImplant(implant);
    return nullptr;
}

PyResult SkillMgrBound::RemoveImplantFromCharacter(PyCallArgs& call, PyInt* itemID)
{
    if (itemID->value() <= 0)
        throw CustomError("That implant is not installed in your active clone.");

    call.client->GetChar()->RemoveImplant(itemID->value());
    return nullptr;
}
