#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "SpellInfo.h"
#include "SpellAuras.h"
#include <unordered_map>

using namespace Acore::ChatCommands;

// Caché global en memoria para guardar las escalas personalizadas (GUID -> Escala)
static std::unordered_map<uint32, float> PlayerScaleCache;

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

        LoadAndApplyScale(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        // Limpiar la memoria cuando el jugador se desconecta
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
            
            // Guardar en el caché de memoria
            PlayerScaleCache[player->GetGUID().GetCounter()] = scale;

            // Aplicar solo si no está transformado en forma de Druida u otro
            if (player->GetShapeshiftForm() == FORM_NONE)
            {
                player->SetObjectScale(scale);
            }
        }
    }
};

// UnitScript para interceptar la pérdida de la transformación
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

        // Verificamos si el aura que se acaba de eliminar es un aura de transformación (Shapeshift)
        SpellInfo const* spellInfo = aurApp->GetBase()->GetSpellInfo();
        if (spellInfo && spellInfo->HasAura(SPELL_AURA_MOD_SHAPESHIFT))
        {
            uint32 guid = player->GetGUID().GetCounter();
            
            // Si tenemos la escala guardada en memoria, la restauramos inmediatamente
            auto it = PlayerScaleCache.find(guid);
            if (it != PlayerScaleCache.end())
            {
                float savedScale = it->second;
                player->SetObjectScale(savedScale);
            }
        }
    }
};

// Comando de Chat .character_scale
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

        // 1. Guardar o actualizar en la base de datos para persistencia a largo plazo
        CharacterDatabase.Execute(
            "INSERT INTO character_scale (guid, scale) VALUES ({}, {}) ON DUPLICATE KEY UPDATE scale = {}",
            player->GetGUID().GetCounter(), scale, scale
        );

        // 2. Actualizar inmediatamente en el caché de memoria
        PlayerScaleCache[player->GetGUID().GetCounter()] = scale;

        // 3. Aplicar al personaje (si no está transformado)
        if (player->GetShapeshiftForm() == FORM_NONE)
        {
            player->SetObjectScale(scale);
            handler->SendSysMessage(Acore::StringFormat("Escala del personaje establecida a {}", scale));
        }
        else
        {
            handler->SendSysMessage(Acore::StringFormat("Escala {} guardada en memoria. Se aplicará al volver a la forma normal.", scale));
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