/*
  ==============================================================================

   Globals that the whole Host relies on.

   Every Host source file except main.cpp is compiled twice: once into the
   standalone application and once into the VST3 plugin. The plugin build has
   no JUCEApplication running, so all the process wide helpers that the graph,
   the editor panels and the windows use have to live in their own translation
   unit and must work in both worlds.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/** Installs the ApplicationProperties owned by the standalone application.

    The standalone app calls this as soon as its settings file is ready. The
    plugin build never calls it, in which case a lazily created process wide
    fallback is used instead.
*/
void setHostAppProperties (ApplicationProperties*);

/** The Host's settings. */
ApplicationProperties& getAppProperties();

/** The Host's command manager. */
ApplicationCommandManager& getCommandManager();

bool isOnTouchDevice();

//==============================================================================
enum class AutoScale
{
    scaled,
    unscaled,
    useDefault
};

constexpr bool autoScaleOptionAvailable =
   #if JUCE_WINDOWS && JUCE_WIN_PER_MONITOR_DPI_AWARE
    true;
   #else
    false;
   #endif

AutoScale getAutoScaleValueForPlugin (const String&);
void setAutoScaleValueForPlugin (const String&, AutoScale);
bool shouldAutoScalePlugin (const PluginDescription&);
void addPluginAutoScaleOptionsSubMenu (AudioPluginInstance*, PopupMenu&);

enum class ConnectionStyle
{
    Bezier,
    Orthogonal
};

ConnectionStyle getConnectionStyle();
void setConnectionStyle (ConnectionStyle);