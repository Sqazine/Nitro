# Nitro
A series of audio effect plugins collection

## Build
```sh
install CMake(version >= 3.22)
install C++ Compiler(GCC,Clang,MSVC etc)

git submoddule update --init --recursive
mkdir build
cd build
cmake ..
cmake --build .

#then executable and vst3 plugin all listed in (build/Bin/) folder 
```

## Host

`Host` comes out of a single `juce_add_plugin` call with two formats:

* `Host_Standalone` -> `Host.exe`, which embeds every plugin of the collection
* `Host_VST3` -> `Host.vst3`, which hosts the same plugin graph, so the collection can be loaded from inside another host

The standalone target keeps the Host's own `JUCEApplication` (`main.cpp`) by defining
`JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`, instead of using JUCE's default standalone shell.
`main.cpp` only holds the application class, every other source file of the `Host` folder is shared
by both flavours. Everything they both depend on (settings, command manager, auto-scale helpers)
lives in `Host/HostAppContext.*`.

Build only one or several plugins can set options on CMake GUI,like:

![alt text](image.png)

or set command like _DBUILD_XXX=ON/OFF to enable/disable plugin build,like:

```sh
cmake -DBUILD_Chorus=ON -DBUILD_JCM800=OFF ..
```

## Create(or Delete) a plugin

I use a python script to manage plugin creation or deletion,this script manage plugin folder, regist to main CMake script automatically

```sh
install python3

python3 plugin_helper [option(-c(or --create),-d(or --delete))] [plugin_name] #such as:python3 plugin_helper -c Delay
```

## References
[JUCE-Official-Tutorials](https://juce.com/learn/tutorials/)

[Audio-Effects](https://github.com/juandagilc/Audio-Effects)

[Audio-Plugin-Development-Resources](https://github.com/jareddrayton/Audio-Plugin-Development-Resources)

[awesome-juce](https://github.com/sudara/awesome-juce)