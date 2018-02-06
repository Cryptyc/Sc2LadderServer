# Sc2LadderServer
A ladder server for SC2 API.

New proxy interface is now implemented, the DLL instructions are out of date.  I will update the instructions within a few days

--------

This will load bots as defined in the LadderBots.json file

The `./data/` directory is writable, you should create a subdirectory with your bot name if you want to write data files.

Requires Blizzard API files (see below).

------

CommandCenter bots are supported.  Just add the json config file in the command center directory with the .ccbot extension.

LadderManager will read a config file from the LadderManager.conf file with the following values

| Config Entry Name | Description |
|---|---|
| `ErrorListFile`	    	|	Place to store games where errors have occured |
| `BotConfigFile`	    	|	Location of the json file defining the bots |
| `MaxGameTime`	    		|	Maximum length of game |
| `CommandCenterDirectory`	|	Directory to read .ccbot command center config files |
| `LocalReplayDirectory`	|	Directory to store local replays |
| `MapListFile`				|	Location of the map list file.  Should be each map on a single line |
| `UploadResultLocation`	|	Location of remote server to store results |

## Developer Install / Compile Instructions (Windows)

* Download and install [Visual Studio 2017](https://www.visualstudio.com/downloads/)
* Have the [StarCraft II AI API Precompiled Libraries](https://github.com/Blizzard/s2client-api#precompiled-libs) ready (extracted) on your PC (its location path on your PC is henceforth referred to as `SC2API`) and the maps installed.
  * To install the maps, download the map packs and follow the directions from [Blizzard's SC2Client-Proto project](https://github.com/Blizzard/s2client-proto#downloads).
* Open the `LadderManager.vcxproj` (found in the root directory of the SC2LadderServer) in Visual Studio

* Go to *Build* (top menu) > *Configuration Manager* and set the configuration to **Release** and the platform to **x64**

* Set the VS Project *include* and *lib* folders to point to the SC2API directories
  * Right click the LadderManager project (not the Solution) in VS2017
  * Make sure you're in Configuration: **Release** and Platform: **x64**
  * Select *Properties*
  * Select *VC++ Directories* on the left
  * Select the *Include Directories* option in the table on the right
  * Click the dropdown arrow on the right and click *Edit...*
  * Add a new directory entry to point to your `SC2API/include` directory
  * Add a new directory entry to point to `Sc2LadderServer/curl/include`
  * Select the *Library Directories* option in the table on the right
  * Click the dropdown arrow on the right and click *Edit...*
  * Add a new directory entry to point to your `SC2API/lib` directory
  * Add a new directory entry to point to `Sc2LadderServer/curl/bin`

* Configure the Linker
  * Check that the linker has x64 as its target: From the project properties, go to *Linker* -> *Advanced* -> *Target Machine* -> *MachineX64*
  * Add the `curl.dll` to the build output: Again in the properties, go to *Build Events* > *Post-Build Event* and add the following as a new entry to *Command Line*: `copy $(SolutionDir)curl\bin\* $(SolutionDir)$(IntDir)`
  * Add another line to the *Post-Build Event*: `copy $(SolutionDir)LadderManager.conf $(SolutionDir)$(IntDir)` - From this point on, after each build, the `LadderManager.conf` from your project root directory will be copied into the directory with the server binary where it is needed. Keep that in mind if you change the config!
 
* From the *Build* menu, click *Build Solution*
  * The build should run through. If not, please check if all steps above were followed.
  * In the directory with the project's `LadderManager.vcxproj`, a new directory `Release` or `x64/Release` should have been created, containing the `LadderManager.exe`

* Update the `LadderManager.conf` (the one in the project root) to fit to your system. The entries are described in the table above. The following steps referring to the entries made here are:
  * Insert the DLLs of the bots you want to run against each other into the `DllDirectory`
  * Create the `MapListFile` as a plain text file. For each map you want the bots to play on, add its name into this file, **including** the `.SC2Map` file ending. Each map comes in its own line.
  * Run *Build* > *Rebuild Solution* to have the updated configuration alongside your `.exe` in the `x64/Release` directory. (*Build Solution* instead of *Rebuild* will not work here, as it only builds if there were changes in the code.)
  
* Run the Sc2LadderServer by double-clicking on the `LadderManager.exe`. If it closes too fast (most likely because you have an error in your configuration) and you want to see what's going on, open up a command line window, navigate to the `Release` directory and enter `LadderManager.exe`.

If you also need a **Debug** build target, please re-do all of the above steps while always ensuring to be in `Debug`.
