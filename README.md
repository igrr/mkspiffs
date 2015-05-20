# mkspiffs
Tool to build and unpack [SPIFFS](https://github.com/pellepl/spiffs) images.



## Usage

```

   ./make_spiffs  {-c <pack_dir>|-l} [-s <number>] [--] [--version] [-h]
                  <image_file>


Where: 

   -c <pack_dir>,  --create <pack_dir>
     (OR required)  create spiffs image from a directory
         -- OR --
   -l,  --list
     (OR required)  list spiffs image to a directory


   -s <number>,  --fs_size <number>
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

## License

MIT

## To do

- Error handling
- Block and page size are hardcoded
- Determing the image size automatically when opening a file
- Unpack
- Automated builds
- Code cleanup

