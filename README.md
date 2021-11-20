# Ledger Free TON app

## Overview
Free TON Ledger app for Ledger Nano S/X

Video demonstration:

[![Free TON application on Leger Nano S](https://i.imgur.com/1X3wW9n.png)](https://youtu.be/QVHLJHUNEJU "Free TON application on Leger Nano S")

## Building and installing
To build and install the app on your Ledger Nano S you must set up the Ledger Nano S build environments. Please follow the Getting Started instructions at [here](https://ledger.readthedocs.io/en/latest/userspace/getting_started.html).

If you don't want to setup a global environnment, you can also setup one just for this app by sourcing `prepare-devenv.sh` with the right target (`s` or `x`).

Install prerequisite and switch to a Nano S dev-env:

```bash
sudo apt install gcc make gcc-multilib g++-multilib libncurses5
sudo apt install python3.8-venv python3.8-dev libudev-dev libusb-1.0-0-dev

# (s or x, depending on your device)
source prepare-devenv.sh s 
```

To fix problems connecting to Ledger follow this [guide](https://support.ledger.com/hc/en-us/articles/115005165269-Fix-connection-issues)

Compile and load the app onto the device:
```bash
make load
```

Refresh the repo (required after Makefile edits):
```bash
make clean
```

Remove the app from the device:
```bash
make delete
```

Enable debug mode:
```bash
make clean DEBUG=1 load
```

## Example of Ledger wallet functionality

All examples are written for Linux and assumes Python 3.8 is installed as default python interpreter

For Mac OS and Windows prepare environment by running `python -m pip install -r requirements.txt` and skip execution of `source prepare-devenv.sh s`. For Mac OS use python3 instead of python.

**Important**: CLI supports only Safe Multisig Wallet (contract number 0; more information about in [`documentation`](doc/freetonapp.asc))

**Request public key:**
```bash
source prepare-devenv.sh s
python freetoncli.py --getpubkey --account 0
```

**Request address for Safe Multisig Wallet:**
```bash
source prepare-devenv.sh s
python freetoncli.py --getaddr --account 1 --contract 0
```

**Deploy Safe Multisig Wallet:**

```bash
source prepare-devenv.sh s
python freetoncli.py --getaddr --account 0 --contract 0 # get future address of the Safe Multisig contract for account 0
# send some tokens to the received address (about 0.5 should be enough)
python freetoncli.py --account 0 --contract 0 --balance # check balance before deploy
python freetoncli.py --url https://main.ton.dev --account 0 --contract 0 deploy
# you will be asked for signature on device
```
Now you can send some tokens from the newly created address

**Send tokens from Safe Multisig Wallet:**

```bash
source prepare-devenv.sh s
python freetoncli.py --url main.ton.dev --account 0 --contract 0 send --dest 0:b3e44db0197dff175f5b71e1003bd57d2e8068892839874eefc8ca95106a8435 --value 0.1
# you will be asked for signature on device
```

## Documentation
This follows the specification available in the [`freetonapp.asc`](doc/freetonapp.asc)
