# S1s: A Call of Duty®: Advanced Warfare® Modded Offline Client

S1s is a client modification based on [s1-mod][s1mod-link] to play Advanced Warfare multiplayer offline against bots, levelling up like in online multiplayer.

**NOTE**: You must legally own Call of Duty®: Advanced Warfare® to run this mod. Cracked/Pirated versions of the game are **NOT** supported.

## Features

- Only compatible with the latest Steam version.

- Independent weapon and world FOV up to 120.
- Bots join the lobby like real players.
- Change the game mode, options, map and team in the public lobby.
- Lobby bots with random outfits and weapons.
- Supply drops and store enabled, with CoD Points earned by playing matches.
- Duplicate supply drop items are converted into CoD Points.
- Sound menu option to mute medal sounds during matches.
- Permanent double XP for player and weapons.
- Offline only.

## Installation

Required files:
```
├── s1_s_mp.exe
└── data/
    └── (useful files)
```

Move all files to your Call of Duty®: Advanced Warfare® game folder.

## Build

- Install [Visual Studio 2022][vs-link] and enable `Desktop development with C++`
- Clone this repository using [Git][git-link]
- Run `generate.bat` (updates the submodules and generates the solution with Premake)
- Build the project via the solution file in `build\s1s.sln`, or run `build.ps1` to build Release x64 from the command line

The output executable is `s1_s_mp.exe`. Only x64 is supported.

### Premake arguments

| Argument                    | Description                                    |
|:----------------------------|:-----------------------------------------------|
| `--copy-to=PATH`            | Optional, copy the EXE to a custom folder after build, define the path here if wanted. |
| `--dev-build`               | Enable development builds of the client. |

## Credits

- [AlterWare][alterware-link] and the s1-mod contributors for the base client.
- Mod by joseupp.

## Disclaimer

This software has been created purely for the purposes of
academic research. It is not intended to be used to attack
other systems. Project maintainers are not responsible or
liable for misuse of the software. Use responsibly.

[s1mod-link]:             https://git.alterware.dev/alterware/s1-mod
[alterware-link]:         https://alterware.dev
[git-link]:               https://git-scm.com
[vs-link]:                https://visualstudio.microsoft.com/vs
