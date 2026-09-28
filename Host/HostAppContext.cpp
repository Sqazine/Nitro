/*
  ==============================================================================

   See HostAppContext.h for a description of why this file exists.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "HostAppContext.h"

//==============================================================================
namespace
{
    ApplicationProperties* hostAppProperties = nullptr;
    std::unique_ptr<ApplicationProperties> fallbackAppProperties;

    ApplicationCommandManager& getProcessCommandManager()
    {
        static ApplicationCommandManager commandManager;
        return commandManager;
    }
}

void setHostAppProperties (ApplicationProperties* properties)
{
    hostAppProperties = properties;
}

ApplicationProperties& getAppProperties()
{
    if (hostAppProperties != nullptr)
        return *hostAppProperties;

    if (fallbackAppProperties == nullptr)
    {
        PropertiesFile::Options options;
        options.applicationName     = "Juce Audio Plugin Host";
        options.filenameSuffix      = "settings";
        options.osxLibrarySubFolder = "Preferences";

        fallbackAppProperties = std::make_unique<ApplicationProperties>();
        fallbackAppProperties->setStorageParameters (options);
    }

    return *fallbackAppProperties;
}

ApplicationCommandManager& getCommandManager()
{
    return getProcessCommandManager();
}

bool isOnTouchDevice()
{
    static bool isTouch = Desktop::getInstance().getMainMouseSource().isTouch();
    return isTouch;
}

//==============================================================================
static AutoScale autoScaleFromString (StringRef str)
{
    if (str.isEmpty())                     return AutoScale::useDefault;
    if (str == CharPointer_ASCII { "0" })  return AutoScale::scaled;
    if (str == CharPointer_ASCII { "1" })  return AutoScale::unscaled;

    jassertfalse;
    return AutoScale::useDefault;
}

static const char* autoScaleToString (AutoScale autoScale)
{
    if (autoScale == AutoScale::scaled)    return "0";
    if (autoScale == AutoScale::unscaled)  return "1";

    return {};
}

AutoScale getAutoScaleValueForPlugin (const String& identifier)
{
    if (identifier.isNotEmpty())
    {
        auto plugins = StringArray::fromLines (getAppProperties().getUserSettings()->getValue ("autoScalePlugins"));
        plugins.removeEmptyStrings();

        for (auto& plugin : plugins)
        {
            auto fromIdentifier = plugin.fromFirstOccurrenceOf (identifier, false, false);

            if (fromIdentifier.isNotEmpty())
                return autoScaleFromString (fromIdentifier.fromFirstOccurrenceOf (":", false, false));
        }
    }

    return AutoScale::useDefault;
}

void setAutoScaleValueForPlugin (const String& identifier, AutoScale s)
{
    auto plugins = StringArray::fromLines (getAppProperties().getUserSettings()->getValue ("autoScalePlugins"));
    plugins.removeEmptyStrings();

    auto index = [identifier, plugins]
    {
        auto it = std::find_if (plugins.begin(), plugins.end(),
                                [&] (const String& str) { return str.startsWith (identifier); });

        return (int) std::distance (plugins.begin(), it);
    }();

    if (s == AutoScale::useDefault && index != plugins.size())
    {
        plugins.remove (index);
    }
    else
    {
        auto str = identifier + ":" + autoScaleToString (s);

        if (index != plugins.size())
            plugins.getReference (index) = str;
        else
            plugins.add (str);
    }

    getAppProperties().getUserSettings()->setValue ("autoScalePlugins", plugins.joinIntoString ("\n"));
}

static bool isAutoScaleAvailableForPlugin (const PluginDescription& description)
{
    return autoScaleOptionAvailable
          && (description.pluginFormatName.containsIgnoreCase ("VST")
              || description.pluginFormatName.containsIgnoreCase ("LV2"));
}

bool shouldAutoScalePlugin (const PluginDescription& description)
{
    if (! isAutoScaleAvailableForPlugin (description))
        return false;

    const auto scaleValue = getAutoScaleValueForPlugin (description.fileOrIdentifier);

    return (scaleValue == AutoScale::scaled
              || (scaleValue == AutoScale::useDefault
                    && getAppProperties().getUserSettings()->getBoolValue ("autoScalePluginWindows")));
}

void addPluginAutoScaleOptionsSubMenu (AudioPluginInstance* pluginInstance,
                                       PopupMenu& menu)
{
    if (pluginInstance == nullptr)
        return;

    auto description = pluginInstance->getPluginDescription();

    if (! isAutoScaleAvailableForPlugin (description))
        return;

    auto identifier = description.fileOrIdentifier;

    PopupMenu autoScaleMenu;

    autoScaleMenu.addItem ("Default",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::useDefault,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::useDefault); });

    autoScaleMenu.addItem ("Enabled",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::scaled,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::scaled); });

    autoScaleMenu.addItem ("Disabled",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::unscaled,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::unscaled); });

    menu.addSubMenu ("Auto-scale window", autoScaleMenu);
}

//==============================================================================
static const char* connectionStyleKey = "connectionStyle";

ConnectionStyle getConnectionStyle()
{
    auto v = getAppProperties().getUserSettings()->getIntValue (connectionStyleKey, 0);
    return (v == 1) ? ConnectionStyle::Orthogonal : ConnectionStyle::Bezier;
}

void setConnectionStyle (ConnectionStyle s)
{
    getAppProperties().getUserSettings()->setValue (connectionStyleKey, (int) s);
    getAppProperties().getUserSettings()->sendChangeMessage();
}