Blinky Tools
============

This package contains a number of tools to work with Blinkies, small sound-to-light conversion sensors.

Install
-------

    pip install blinkytools


Use the GUI
-----------

The GUI can be used to record Blinky signals on live data by
providing the identifier of the video stream

    python -m blinkytools -v <video_stream>

or from a file by providing a file name

    python -m blinkytools -v <video_filename>

When called without argument, the default video stream is used.

The GUI can also be used with a
[Shodensha  DN3V-130BU](https://www.shodensha-inc.co.jp/ja/dn3v-130bu/)
industrial camera, but this requires to install the driver and python bindings
for the driver from [here](https://github.com/onolab-tmu/icube_sdk).

    python -m blinkytools -i

Preview the content of a file
-----------------------------

The package can also be used to preview the content
of a file recorded using the GUI.

    python -m blinkytools.file_preview <filename>

Open file from within Python
----------------------------

The file format is [MessagePack](https://msgpack.org) and can thus be read by
any appropriate software package.
The content is as follows:

* Software version
* File creation date
* Framerate of the signal
* A dictionary of extra metadata
* The Blinky signals stored as a binary data array

This is an example of script that opens a Blinky file.

    from blinkytools import BlinkyFile

    # open the file and load the content
    filename = "myfile.blinky"
    content = BlinkyFile.load(filename)

    print("Software version used:", content.version)
    print("Framerate:", content.fps)
    print("File created on:", content.creation)
    print("Dictionary of extra metadata:", content.metadata)

    # The Blinky signal are in the data attribute
    print("The data shape and type:", content.data.shape, content.data.dtype)

The dimension of the `content.data` array is as follows.

* For **monochrome** cameras, the array has 4 dimensions, with `shape ==
  (n_time, n_blinkies, h_patch, w_patch)`, where `n_time` and `n_blinkies` are
  the number of frames and Blinky sensors, respectively. The data is exctraced
  in small patches of `h_patch x w_patch` around the Blinky locations.
* For **color** cameras, the array has 5 dimensions, with `shape == (n_time,
  n_blinkies, h_patch, w_patch, n_colors)`, where `n_colors` is the number of
  color channels.

### Note on storage of Numpy arrays in MessagePack

When using the `BlinkyFile` class, the signals are automatically transformed into `numpy.ndarray`.
The arrays are stored in MessagePack format as the following dictionary:

    {
      "__nd__": True,  # indicates this is a numpy ndarray
      "shape": obj.shape,  # shape of the array
      "dtype": obj.dtype.str,  # numeric type of the data
      "data": obj.tobytes(),  # the data as a byte array
    }

Note that this information is only needed if dealing with the content of the file directly.l
When using `blinkytools`, the conversion is transparent.
