# pd-musesampler~

Use MuseSampler inside PureData.

> [!WARNING]
> You need to install MuseSounds. Check the instructions [here](https://musescore.org/en/handbook/4/installing-muse-sounds).

# Build

For Linux, Mac and Windows (using Mingw64):

1. `git clone https://github.com/charlesneimog/pd-musesampler/ --recursive`;
2. `cd pd-musesampler`;
3. `cmake . -B build`;
4. `cmake --build build`;

The binaries will be inside the folder `pd-musesampler`, inside this folder, open the `musesampler~-help.pd`.

# License

This object needs `MuseSounds` developed by Muse Group. Download it from https://www.musehub.com/ and for Linux search for Muse Sounds Manager (bottom of the page).

As they say: 

> Muse Sounds cover the symphony orchestra and choral voices. Incredibly, these beautifully crafted instrument packs are available exclusively in Muse Hub completely **free**.​

I hope that this can be used for Live-Eletronics music. Or just for fun!
