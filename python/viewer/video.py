"""
This file contains routines to help dealing with processing
streaming videos and reading frames and pixel locations

Code written by Daiki Horiike and Robin Scheibler, 2018
"""
import cv2
from threading import Thread
import queue


class FrameGrabber(object):
    def __init__(self):
        self.the_frame = None

    def process(self, frame):
        self.the_frame = frame

    def extract(self):
        return self.the_frame


class ThreadedVideoStream(object):
    """
    This class capture a video (either from live stream or file) using
    the opencv api in a separate thread.
    Parameters
    ----------
    video: int or str
        If an int, this is the index of the video stream. If a str, this is a filename.
    """

    def __init__(self, video, start=0, end=None, qmax_len=200):

        # The maximum number of frames to buffer
        self.qmax_len = qmax_len

        # we'll store frames there
        self.queue = queue.Queue()
        self.video_source = video
        self._start = start

        if end == -1:
            self._end = None
        else:
            self._end = end

        # Try to access the device
        self.capture = cv2.VideoCapture(self.video_source)
        if not self.capture:
            raise ValueError("Couldn" "t open the device.")

        if self._start != 0:
            # So CAP_PROP_POS_FRAMES seems to be broken,
            # per https://github.com/opencv/opencv/issues/9053
            self.pos_frames = self._start // 2  # set first frame to read
            print(
                "Warning: setting start frame seems buggy "
                "(https://github.com/opencv/opencv/issues/9053). "
                "A hack was used. Use at your own risk"
            )

        if self._end is not None:
            self._end = self._end - self._start

        """ Start the stream """
        self._stopped = False  # Use a flag to know the state of the process
        self._count = 0
        self.thread = Thread(target=self._frame_read_loop, args=())
        self.thread.start()

    def __len__(self):
        return self.queue.qsize()

    @property
    def is_streaming(self):
        """ Returns True if capture is running, and False otherwise. """
        return not self._stopped

    @property
    def available(self):
        return self.is_streaming and self.queue.qsize() > 0

    @property
    def width(self):
        return int(self.capture.get(cv2.CAP_PROP_FRAME_WIDTH))

    @property
    def height(self):
        return int(self.capture.get(cv2.CAP_PROP_FRAME_HEIGHT))

    @property
    def brightness(self):
        return self.capture.get(cv2.CAP_PROP_BRIGHTNESS)

    @brightness.setter
    def brightness(self, value):
        self.capture.set(cv2.CAP_PROP_BRIGHTNESS, value)

    @property
    def exposure(self):
        return self.capture.get(cv2.CAP_PROP_EXPOSURE)

    @exposure.setter
    def exposure(self, value):
        self.capture.set(cv2.CAP_PROP_EXPOSURE, value)

    @property
    def auto_exposure(self):
        return self.capture.get(cv2.CAP_PROP_AUTO_EXPOSURE)

    @auto_exposure.setter
    def auto_exposure(self, value):
        self.capture.set(cv2.CAP_PROP_AUTO_EXPOSURE, value)

    @property
    def fps(self):
        return self.capture.get(cv2.CAP_PROP_FPS)

    @property
    def shape(self):
        """ The frame shape in numpy format """
        return (self.height, self.width)

    @property
    def pos_frames(self):
        return self.capture.get(cv2.CAP_PROP_POS_FRAMES)

    @pos_frames.setter
    def pos_frames(self, value):
        self.capture.set(cv2.CAP_PROP_POS_FRAMES, value)

    def __del__(self):
        self.stop()  # stop, just in case

    def stop(self):
        """ Stop the stream """
        self._stopped = True
        self.thread.join()  # wait for frame reading loop to stop
        with self.queue.mutex:
            self.queue.queue.clear()

        # close the video feed
        if self.capture.isOpened():
            self.capture.release()

    def read(self, n=1, block=True, timeout=1):
        """
        Read some frames.
        Parameters
        ----------
        n: int
            Number of frames to retrieve
        block: optional, bool
            Wether to do a blocking call or not
        """

        if self.queue.qsize() == 0 and self._stopped:
            return None

        if n == 1:
            while not self._stopped:
                try:
                    return self.queue.get(block=block, timeout=timeout)
                except queue.Empty:
                    if self._stopped:
                        return None

        elif n > 1:
            ret = []

            while len(ret) < n:
                try:
                    ret.append(self.queue.get(block=block, timeout=timeout))
                except queue.Empty:
                    if self._stopped:
                        return ret

            return ret

        else:
            raise ValueError("n must be strictly positive")

    def _frame_read_loop(self):
        """ This method will fetch the frames in a concurrent thread """

        while not self._stopped:

            if self.queue.qsize() >= self.qmax_len:
                continue

            ret, frame = self.capture.read()

            self._count += 1

            if not ret:
                break
            else:
                # Return RGB frame
                self.queue.put(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
                if self._end is not None and self._count >= self._end:
                    break

        self._stopped = True

    def __enter__(self):
        pass

    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()


def video_stream(video, start=0, end=None, callback=None, show=False):
    """
    Streams a video for display or processing

    Parameters
    ----------
    video: int or str
        If an int, this is the index of the video stream. If a str, this is a filename.
    start: int
        The frame number where to start the streaming
    end: int
        The last frame to stream
    callback: func
        A function to call on each frame
    show: bool
        If True, the video is displayed during streaming
    """

    if show:
        cv2.namedWindow("image", cv2.WINDOW_AUTOSIZE)

    with ThreadedVideoStream(video, start=start, end=end) as cap:

        fps = cap.get_fps()

        while cap.is_streaming:

            frame = cap.read()
            if frame is None:
                break

            if show:
                cv2.imshow("image", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            if callback is not None:
                callback(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))

    if show:
        cv2.destroyAllWindows()

    return fps


def frame_grabber(video, frame=0, show=False):
    """
    Retrieve a single frame from a video file or stream

    Parameters
    ----------
    video: int or str
        If an int, this is the index of the video stream. If a str, this is a filename.
    frame: int, optional
        The frame index to grab
    show: bool, optional
        Display the frame using matplotlib
    """

    grabber = FrameGrabber()
    video_stream(video, start=frame, end=frame, callback=grabber.process)

    if show:
        import matplotlib.pyplot as plt
        import numpy as np

        plt.imshow(np.array(grabber.extract()))
        plt.show()

    return grabber.extract()


class MouseParam(object):
    """ Helper object to get a mouse click coordinates in an image """

    def __init__(self, input_img_name):
        # parameters of the mouse click
        self.mouse_event = {"x": None, "y": None, "event": None, "flags": None}
        # setting of the mouse callback
        cv2.setMouseCallback(input_img_name, self.__callback, None)

    # callback function
    def __callback(self, event_type, x, y, flags, userdata):

        self.mouse_event["x"] = x
        self.mouse_event["y"] = y
        self.mouse_event["event"] = event_type
        self.mouse_event["flags"] = flags

    def get(self, param_name):
        return self.mouse_event[param_name]

    def get_pos(self):
        return (self.mouse_event["x"], self.mouse_event["y"])


def pixel_from_click(frame):
    """
    Obtain the location of a pixel clicked with the mouse in a frame
    """
    window_name = "input window"
    cv2.imshow(window_name, frame)

    mouse_data = MouseParam(window_name)

    while 1:
        cv2.waitKey(20)
        if mouse_data.get("event") == cv2.EVENT_LBUTTONDOWN:  # left click
            y, x = mouse_data.get_pos()
            break

    cv2.destroyAllWindows()
    return x, y


if __name__ == "__main__":

    import argparse

    parser = argparse.ArgumentParser(description="Video loader/stream player")
    parser.add_argument("video", default=0, type=str, help="Stream index or file name")
    parser.add_argument(
        "--display",
        "-d",
        action="store_true",
        help="Displays the video as it is streamed",
    )
    args = parser.parse_args()

    try:
        video = int(args.video)
    except:
        video = args.video

    # Start capture
    video_stream(video, show=args.display)
