# mkspiffs
Tool to build and unpack [SPIFFS](https://github.com/pellepl/spiffs) images.



## Usage

```

   ./mkspiffs  {-c <pack_dir>|-l|-i} [-b <number>] [-p <number>] [-s
               <number>] [--] [--version] [-h] <image_file>


Where:

   -c <pack_dir>,  --create <pack_dir>
        create spiffs image from a directory
         -- OR --
   -l,  --list
        list files in spiffs image
         -- OR --
   -i,  --visualize
        visualize spiffs image


   -b <number>,  --block <number>
     fs block size, in bytes

   -p <number>,  --page <number>
     fs page size, in bytes

   -s <number>,  --size <number>
     fs image size, in bytes

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.

   <image_file>
     (required)  spiffs image file


```
## Build

You need gcc (>= 4.8) and make. On Windows, use MinGW.

Run:
```bash
$ make dist
```

### Build status

Linux | Windows
------|-------
 [![Linux build status](http://img.shields.io/travis/igrr/mkspiffs.svg)](https://travis-ci.org/igrr/mkspiffs) | [![Windows build status](http://img.shields.io/appveyor/ci/igrr/mkspiffs.svg)](https://ci.appveyor.com/project/igrr/mkspiffs)


## License

MIT

## To do

- Error handling
- Determine the image size automatically when opening a file
- Code cleanup
