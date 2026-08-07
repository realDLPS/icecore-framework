# icpak is a package format and packer combo that I am exploring and designing for Icecore Framework (icfw).

This is very much a work in progress file about my thoughts on how to do a system like this.


# Format
## Table of Contents (TOC)
- Is a separate file .ictoc
- Contains information about what assets the icpaks contain
    - Tells which icpak index the asset manager should look into for a specific asset
        - After which the asset manager can look into the ICPAK index to find the asset header
    - This is done to make finding an asset faster instead of relying on luck to get the right icpak index on the first try
- Contains the hash of ICPAK indecies to identify corruption or modification
- Contains information about the following
    - TOC Version
    - ICPAK version
    - ICPAK index version
    - Asset format version
    - Asset header version
    - How large is the block multiple (ie 1 MiB, 512 kiB etc)
- The binary holds the hash for the TOC that it is expecting
- Worth noting is that the user decides which icpak an asset gets put into.

## ICPAK
- Contains asset block(s)
## ICPAK Index
- Contains information about an ICPAK file
    - Block count
    - Block sizes (How many MiB a block is)
    - Block hashes (To identify if the block is still intact and hasn't been corrupted or modified)
    - Map of assets
        - Key: Asset UUID (This is used to identify an asset even through name and content changes, and is generated once upon asset creation)
        - Content:
            - Asset header
            - Block location (index of the block the asset is in)
## (Asset) Block
- By design these try to remain as unchanged as possible when assets are changing to facilitate steampipe working as well as it can.
- By default a block is 1 MiB and contains multiple assets, but if a compressed asset is above 1 MiB it will cause a block that is a larger multiple of 1 MiB to be created instead.
    - This means that a single asset is always inside a single block.
    - The block size of 1 MiB is the default, but this will be a config variable for a project and could for example be 512 KiB if you used another system that isn't steampipe that wanted 512 KiB as a block size.
- A block can contain "dead assets"
    - This is further discussed in the packer section

## Asset
- Bytes that can be streamed or loaded at once

## Asset header
- Contains information about an asset
    - Offset inside the block the asset is stored in
    - Compressed size of the asset
    - Hash of the compressed asset (To identify if the compressed bytes have been corrupted or changed)
    - Uncompressed size of the asset
    - Hash of the uncompressed asset (To identify if the decompression produced the expected bytes)
    - Asset type
    - Compression type
    - Asset flags

## In short
- There are two co-existing file types
    - Headers
        - ICPAK indecies that contains Asset headers
    - Content
        - ICPAKs that contain Assets
- Each file type also starts with a magic number to identify it
    - Number are to be defined.

# .icast
- TBD, but is a development time format that holds more information less efficiently than the packed formats used at runtime after packing
    - Holds actual paths to assets
    - Holds what icpak the asset will go into

# Packer
- The packer handles creation of icpaks from .icast files. 
- While packing the packer tries to fit multiple assets into a single block, and upon doing so adds them to a list of "block-buddies"
    - Block-buddies are assets that are stored in the same block. To avoid needless updating an asset might end up being turned into a "dead asset"
    - The packer will put each asset into the first new block it fits into
    - A block will not accept new assets after creation
        - The packer will not place new assets (or moving assets) into blocks which were already present in the previous packing
- Dead assets
    - Dead assets are assets that were either deleted entirely or had to be moved out of a block, for example due to it resizing.
        - Basically meaning that these bytes still exist, but nothing in the TOC leads to them.
        - The space of dead assets doesn't get reused and will persist as the same bytes until a purge deletes the block.
    - These will be eventually purged either automatically or manually.
        - This depends on a parameter of how much of the used space needs to still be used live assets
            - Example (parameter: 50%):
                - A: 100kb, B: 100kb, C: 300kb <- Initial state
                    - A(dead): 100kb, B(dead): 100kb, C: 300kb -> No purging
                    - A: 100kb, B: 100kb, C(dead): 300kb -> Purged
            - Example (parameter: 90%):
                - A: 100kb, B: 100kb, C: 300kb <- Initial state
                    - A(dead): 100kb, B: 100kb, C: 300kb -> Purged
            - Example (parameter: 20%):
                - A: 100kb, B: 100kb, C: 300kb <- Initial state
                    - A(dead): 100kb, B(dead): 100kb, C: 300kb -> No purging
                    - A: 100kb, B: 100kb, C(dead): 300kb -> No purging
                    - A: 100kb, B(dead): 100kb, C(dead): 300kb -> No purging
                    - A(dead): 100kb, B(dead): 100kb, C(dead): 300kb -> Purged
        - Purging means that all live assets are moved into new blocks and this block is deleted
- The packer is able to place blocks in any order as steampipe only cares about one continous 1 MiB of memory as lon as it can be found else where within a file aligned to 1 MiB.
    - This means that upon a purge later blocks can be moved to occupy the space the now purged block took up in the icpak.
- Purging
    - The packer features automatic purging as mentioned previously
    - Manual purging via an argument passed to the packer that purges all blocks or blocks with dead assets (decided by the passed argument)
    - The packer also has a waste space threshold, which if passed will cause an entire purge of blocks containing dead assets
        - For example if the threshold is 20% and that is reached all blocks with dead assets will get purged
- The packer functions by having two depots (basically versions of the packed content)
    - Debug, the debug depot contains assets you might not want to end up in release builds
        - Upcoming content etc
    - Release
        - Content that will be released to users in this build or has been available to them before
    
    The security reasoning for this is to allow testing of packing during development without unreleased content accidentally leaking through
    dead assets in blocks that ended up in there during development. 
    
    **The packer will never pull blocks between depots** (and the default icfw build system will never copy in the built debug depot into a release build)
- The packer also maintains depot manifests.

## Depot manifest
- Holds information about assets in the depot
- Map of asset uuids to asset hashes
    - This is used to detect when an asset has changed
- Holds block-buddy information
- Only used by the packer and not included in builds.

## Block-buddy
- Holds a list of uuids beloging in the same block
- Upon each packing the packer checks how many of the assets in the block are still alive and if the block should be purged.