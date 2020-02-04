"""
The objects defined in this file are meant to run
some kind of processing on a video in a streaming fashion.

They should have a `process` method that can get called on
a frame or a stack of frame. A frame is a 3d-array of size
(height, width, n_colors), where n_colors is the number of color
channels, usually this is 3 (RGB). A stack of frame has one or more
extra dimensions in the front
"""

import abc
import numpy as np
from collections import deque
from threading import Thread
import queue
import time


class ProcessorBase(abc.ABC):
    """
    Base class for online processing objects

    The derived classes should implement the __process__ method.
    """

    def __init__(self, monitor=False, qlen=10):
        self.monitor = monitor

        self.deltas = deque([], qlen)  # list of time delta between two batch of frames
        self.avg_fps = 0.0

        self.monitor_freq = 0.5  # print new message every second

        self.total_frames = 0
        self.start = time.perf_counter()
        self.runtime = 0.0

        self.queue = queue.Queue()

        self.frames_since_last_call = 0
        self.last_print = self.start

        # attributes related to the threaded processing
        self._is_running = True
        self._thread = Thread(target=self._process_loop, args=())
        self._thread.start()

    def stop(self):
        """ Interface to stop the thread """
        # stop the processing
        self._is_running = False

        # wait for processing loop to terminate
        self._thread.join()

        # do the final processing
        self.__finalize__()

    def process(self, frames):
        """ Interface to add new frames for processing """
        if self._is_running:
            self.queue.put(frames)

    def __len__(self):
        return self.queue.qsize()

    @property
    def fps(self):
        """ Number of frames processed per second """
        return self.total_frames / self.runtime if self.runtime > 0 else 0

    def print_perf(self):
        """ Prints the number of frames processed per second to standard output """
        print(
            "{} frames processed in {} seconds (fps: {:7.3f})".format(
                self.total_frames, self.runtime, self.fps
            )
        )

    @abc.abstractmethod
    def __finalize__(self):
        """
        Abstract method called at the end of the processing
        It can be called to save the data to file, etc
        """
        pass

    @abc.abstractmethod
    def __process__(self, frames):
        """
        Abstract method called to process the frames
        """
        pass

    def _process_loop(self):
        """ The processing loop running in a separate thread """

        while self._is_running:

            try:
                frames = self.queue.get(block=True, timeout=0.1)
            except queue.Empty:
                continue

            # do the work
            self.__process__(frames)

            if self.monitor:

                # now measure the frame rate
                if frames.ndim <= 3:
                    n_frames = 1
                elif frames.ndim > 3:
                    n_frames = np.prod(frames.shape[:-3])

                self.frames_since_last_call += n_frames
                self.total_frames += n_frames

                now = time.perf_counter()

                delta = now - self.last_print
                self.runtime = now - self.start

                if delta > self.monitor_freq:

                    self.deltas.appendleft((delta, self.frames_since_last_call))

                    avg_fps = np.mean([n / d for d, n in self.deltas])

                    print("avg fps: {:7.3f}".format(avg_fps), end="\r")

                    self.last_print = now
                    self.frames_since_last_call = 0


class ReadSpeedMonitor(ProcessorBase):
    """ A dummy class to monitor the video frame read speed """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def __process__(self, frames):
        pass

    def __finalize__(self):
        pass


class OnlineStats(ProcessorBase):
    """
    Compute statistics on the input data in an online way

    Parameters
    ----------
    shape: tuple of int
        Shape of a data point tensor

    Attributes
    ----------
    mean: array_like (shape)
        Mean of all input samples
    var: array_like (shape)
        Variance of all input samples
    count: int
        Sample size
    """

    def __init__(self, shape, monitor=False, qlen=10):
        """
        Initialize everything to zero
        """
        # call parent method
        ProcessorBase.__init__(self, monitor=monitor, qlen=qlen)

        self.shape = shape
        self.mean = np.zeros(shape, dtype=np.float64)
        self.var = np.zeros(shape, dtype=np.float64)
        self.count = 0

    def __process__(self, data):
        """
        Update statistics with new data

        Parameters
        ----------
        data: array_like
            A collection of new data points of the correct shape in an array
            where the first dimension goes along the data points
        """

        if data.shape[-len(self.shape) :] != self.shape:
            raise ValueError(
                "The data.shape[1:] should match the statistics object shape"
            )

        data.reshape((-1,) + self.shape)

        count = data.shape[0]
        mean = np.mean(data, axis=0)
        var = np.var(data, axis=0)

        m1 = self.var * (self.count - 1)
        m2 = var * (count - 1)
        M2 = (
            m1
            + m2
            + (self.mean - mean) ** 2 * count * self.count / (count + self.count)
        )

        self.mean = (count * mean + self.count * self.mean) / (count + self.count)
        self.count += count
        self.var = M2 / (self.count - 1)


class BoxCatcher(ProcessorBase):
    """
    This is a simple object that collect the values of a few pixels on the
    stream of frames

    Parameters
    ----------
    pixels: list of tuples
        The location of the pixels to collect in the image
    box_size: list  or tuple of two int
        The width and height of the bounding box to use for averaging
    """

    def __init__(self, pixels, box_size, monitor=False):

        # call parent method
        ProcessorBase.__init__(self, monitor=monitor)

        # set the attributes
        self.pixels = pixels
        self.data = []
        self.box_size = box_size

        # precompute the slices for each pixel
        off_w = self.box_size[0] // 2
        off_h = self.box_size[1] // 2
        self.ranges = [
            [
                slice(loc[0] - off_w, loc[0] - off_w + self.box_size[0]),  # columns
                slice(loc[1] - off_h, loc[1] - off_h + self.box_size[1]),  # row
            ]
            for loc in self.pixels
        ]

    def __process__(self, frames):
        """
        Catch the values of the pixels in a stack of frames

        Parameters
        ----------
        frames: array_like (..., width, height, n_colors)
            The stack of frames
        """
        self.data.append([frames[r_h, r_w].copy() for r_w, r_h in self.ranges])

    def __finalize__(self):
        """
        Consolidate collected data in numpy ndarray with shape (n_frames, n_pixels, box_height, box_width, 
        """
        self.data = np.array(self.data)
