# NDSEventTool - Nintendo DS Mystery Event Distributor

This is based on nds-savegame-manager, a NDS homebrew tool by Pokedoc to rip and restore saves from DS and GBA games.

This modification works with Pokémon GBA games and allows to inject official Nintendo Events to the savegames in the cartridge.

Please, consider making a backup with the standard homebrew by Pokedoc (https://code.google.com/p/savegame-manager/).


The code is published under the GNU GPL v2.

Below is a copy of the project description at https://code.google.com/p/savegame-manager/

## Build Instructions

### Windows

#### Install Dependencies

- Install [cygwin](https://cygwin.com/install.html) for Windows. Make sure to also install the `make` command,
which is not part of the minimal `cygwin` install.


- Install [devkitPro](https://devkitpro.org/wiki/Getting_Started).

#### Build

Open a `command prompt` to this directory and run the `make` command.

To delete all build products, run `make clean`.

### Mac

#### Install Dependencies

- Install [homebrew](https://brew.sh/).

- Install `xz`:

   ```
   brew install xz
   ```

- Install [pacman](https://github.com/devkitPro/pacman/releases/latest) devkitPro utilities for Mac.

- Run the following command to install the NDS (Nintendo DS) tools

   ```
   sudo /opt/devkitpro/pacman/bin/pacman -S nds-dev
   ```
   
   When prompted to install specific options, just press `enter` to default to "all".

#### Configure Environment Variables

Open up `~/.bash_profile`.

Add the following lines:

```
 export DEVKITPRO=/opt/devkitpro
 export DEVKITARM=${DEVKITPRO}/devkitARM
```

Lastly, run the following command to reload the file:

```
source ~/.bash_profile
```

#### Build

Open `terminal` to this directory and run the `make` command.

To delete all build products, run `make clean`.
