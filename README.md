# pd-musesampler~

Use MuseSampler inside PureData.

> [!WARNING]
> You need to install MuseSounds. Check the instructions [here](https://musescore.org/en/handbook/4/installing-muse-sounds).

# Build
> [!NOTE]
> You will need PureData, download it [here](https://puredata.info/downloads/pure-data).

For Linux, Mac and Windows (using Mingw64):

1. `git clone https://github.com/charlesneimog/pd-musesampler/ --recursive`;
2. `cd pd-musesampler`;
3. `cmake . -B build`;
4. `cmake --build build`;

The binaries will be inside the folder `pd-musesampler`, inside this folder, open the `musesampler~-help.pd`.

# License

`pd-musesampler` uses `apitypes.h` from MuseScore. MuseScore is licensed under GPL version 3.0. 
