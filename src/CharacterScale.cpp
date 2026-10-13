#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include "EventProcessor.h"
#include <unordered_map>

using namespace Acore::ChatCommands;

// Caché global en memoria para guardar las escalas personalizadas (GUID -> Escala)
static std::unordered_map<uint32, float> PlayerScaleCache;

// Función auxiliar para identificar exclusivamente las formas de Druida
static bool IsDruidForm(uint32 form)
{
    return form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR ||
           form == FORM_MOONKIN || form == FORM_TREE || form == FORM_TRAVEL ||
           form == FORM_AQUA || form == FORM_FLIGHT || form == FORM_FLIGHT_EPIC;
}

// Evento para aplicar la escala con retraso tras teletransportarse
class ScaleApplyEvent : public BasicEvent
{
public:
    ScaleApplyEvent(Player* player, float scale) : _player(player), _scale(scale) {}

    bool Execute(uint64 /*et*/, uint32 /*pTime*/) override
    {
        if (_player && !IsDruidForm(_player->GetShapeshiftForm()))
        {
            _player->SetObjectScale(_scale);
        }
        return true;
    }

private:
    Player* _player;
    float _scale;
};

class CharacterScaleScript : public PlayerScript
{
public:
    CharacterScaleScript() : PlayerScript("CharacterScaleScript") {}

    void OnPlayerLogin(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("CharacterScale.Enable", true))
            return;

        LoadAndApplyScale(player);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("CharacterScale.Enable", true))
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto it = PlayerScaleCache.find(guid);
        if (it != PlayerScaleCache.end())
        {
            player->m_Events.AddEvent(new ScaleApplyEvent(player, it->second), player->m_Events.CalculateTime(400));
        }
        else
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT scale FROM character_scale WHERE guid = {}", 
                guid
            );

            if (result)
            {
                Field* fields = result->Fetch();
                float scale = fields[0].Get<float>();
                PlayerScaleCache[guid] = scale;
                player->m_Events.AddEvent(new ScaleApplyEvent(player, scale), player->m_Events.CalculateTime(400));
            }
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        PlayerScaleCache.erase(player->GetGUID().GetCounter());
    }

    static void LoadAndApplyScale(Player* player)
    {
        if (!player)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT scale FROM character_scale WHERE guid = {}", 
            player->GetGUID().GetCounter()
        );

        if (result)
        {
            Field* fields = result->Fetch();
            float scale = fields[0].Get<float>();
            
            PlayerScaleCache[player->GetGUID().GetCounter()] = scale;

            if (!IsDruidForm(player->GetShapeshiftForm()))
            {
                player->SetObjectScale(scale);
            }
        }
    }
};

class CharacterScaleUnitScript : public UnitScript
{
public:
    CharacterScaleUnitScript() : UnitScript("CharacterScaleUnitScript") {}

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode /*mode*/) override
    {
        if (!sConfigMgr->GetOption<bool>("CharacterScale.Enable", true))
            return;

        Player* player = unit->ToPlayer();
        if (!player)
            return;

        SpellInfo const* spellInfo = aurApp->GetBase()->GetSpellInfo();
        if (spellInfo && spellInfo->HasAura(SPELL_AURA_MOD_SHAPESHIFT))
        {
            if (!IsDruidForm(player->GetShapeshiftForm()))
            {
                uint32 guid = player->GetGUID().GetCounter();
                auto it = PlayerScaleCache.find(guid);
                if (it != PlayerScaleCache.end())
                {
                    float savedScale = it->second;
                    player->SetObjectScale(savedScale);
                }
            }
        }
    }
};

class CharacterScaleCommand : public CommandScript
{
public:
    CharacterScaleCommand() : CommandScript("CharacterScaleCommand") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "character_scale", HandleSetScaleCommand, SEC_PLAYER, Console::No }
        };
        return commandTable;
    }

    static bool HandleSetScaleCommand(ChatHandler* handler, Optional<float> scaleOpt)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        float minScale = sConfigMgr->GetOption<float>("CharacterScale.MinScale", 0.5f);
        float maxScale = sConfigMgr->GetOption<float>("CharacterScale.MaxScale", 2.0f);

        if (!scaleOpt.has_value())
        {
            handler->SendSysMessage(Acore::StringFormat("Uso: .character_scale <val> (Rango permitido: {} - {})", minScale, maxScale));
            return true;
        }

        float scale = *scaleOpt;

        if (scale < minScale || scale > maxScale)
        {
            handler->SendSysMessage(Acore::StringFormat("Escala fuera de rango. Debe estar entre {} y {}.", minScale, maxScale));
            return true;
        }

    CharacterDatabase.Execute(
            "INSERT INTO character_scale (guid, scale) VALUES ({}, {}) ON DUPLICATE KEY UPDATE scale = {}",
            player->GetGUID().GetCounter(), scale, scale
        );

        PlayerScaleCache[player->GetGUID().GetCounter()] = scale;

        if (!IsDruidForm(player->GetShapeshiftForm()))
        {
            player->SetObjectScale(scale);
            handler->SendSysMessage(Acore::StringFormat("Escala del personaje establecida a {}", scale));
        }
        else
        {
            handler->SendSysMessage(Acore::StringFormat("Escala {} guardada en memoria. Se aplicará al salir de la forma de druida.", scale));
        }

        return true;
    }
};

void Addmod_character_scaleScripts()
{
    new CharacterScaleScript();
    new CharacterScaleUnitScript();
    new CharacterScaleCommand();
}