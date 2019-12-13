import tkinter
from tkinter import (
    Label,
    Button,
    Canvas,
    Scale,
    NW,
    CENTER,
    NE,
    HORIZONTAL,
    E,
    RIGHT,
    LEFT,
)
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

import PIL
import PIL.ImageTk
import numpy as np
import cv2

from .videocapture import VideoCapture


class ZoomCanvas(Canvas):
    def __init__(self, patch_size, zoom_gain, *args, **kwargs):
        self.win_size = patch_size * zoom_gain
        self.patch_size = patch_size
        self.gain = zoom_gain

        self._vignette = None

        super().__init__(width=self.win_size, height=self.win_size, *args, **kwargs)

    def update(self, frame, row, col):

        offset = (self.patch_size - 1) // 2

        cut_out = frame[
            row - offset : row + offset + 1, col - offset : col + offset + 1
        ].copy()

        z = cv2.resize(
            cut_out, (self.win_size, self.win_size), interpolation=cv2.INTER_NEAREST
        )

        self._vignette = PIL.ImageTk.PhotoImage(image=PIL.Image.fromarray(z))
        self.create_image(0, 0, image=self._vignette, anchor=CENTER)


class PixelTracker(object):
    def __init__(
        self,
        row=None,
        col=None,
        buffer_size=100,
        width=500,
        height=400,
        dpi=100,
        master=None,
    ):

        self.fig = Figure(
            figsize=(self.width / self.dpi, self.height / self.dpi), dpi=self.dpi
        )
        self.ax = self.fig.add_subplot(111)
        self.plt_canvas = FigureCanvasTkAgg(self.fig, master=master)
        self.plt_canvas.draw()

        # Attributes for the pixel tracker
        self.row, self.col = row, col
        self.values = [0 for i in range(buffer_size)]

    def pack(self, *args, **kwargs):
        self.plt_canvas.get_tk_widget().pack(*args, **kwargs)

    def reset(self, row, col):
        self.row = row
        self.col = col

    def update(self, frame):

        # Make the value grayscale if it isn't already
        if frame.ndim == 3:
            value = cv2.cvtColor(
                frame[self.row, self.col, None, None, :], cv2.COLOR_RGB2GRAY
            )[0, 0]
        elif frame.ndim == 2:
            value = frame[self.row, self.col]
        else:
            raise ValueError("Bad input")

        self.values.pop(0)
        self.values.append(value)

        # update the graph
        self.ax.clear()
        self.ax.plot(self.pixel_tracker.values)
        self.ax.set_ylim([0, 255])
        self.plt_canvas.draw()



def zoom(frame, row, col, box_size, target_size):

    offset = (box_size - 1) // 2
    cut_out = frame[
        row - offset : row + offset + 1, col - offset : col + offset + 1
    ].copy()

    vignette = cv2.resize(
        cut_out, (target_size, target_size), interpolation=cv2.INTER_NEAREST
    )

    return vignette


def toggle(x: bool):
    return x ^ True


# Now come the GUI part
class BlinkyViewer(object):
    def __init__(self, window, video_source=0):
        self.window = window

        # control
        self.checksat = False
        self.convert_bw = False
        self.convert_log = False

        # zoom size
        self.zoom_patch_size = 51
        self.zoom_gain = 10

        # open video feed
        self.vid = VideoCapture(video_source)

        self.vid_brightness = self.vid.brightness
        self.vid_exposure = self.vid.exposure

        # create canvas of the right size
        self.canvas = Canvas(window, width=self.vid.width, height=self.vid.height)
        self.canvas.bind("<Button-1>", self.mouse_callback)
        self.canvas.pack()

        self.canvas_buttons = Canvas(window)
        self.canvas_buttons.pack(anchor=CENTER, expand=True)
        self.btn_checksat = Button(
            self.canvas_buttons, text="Sat", width=20, command=self.toggle_checksat
        )
        self.btn_checksat.pack(side=LEFT, expand=True)

        self.btn_convert_bw = Button(
            self.canvas_buttons, text="BW", width=20, command=self.toggle_convert_bw
        )
        self.btn_convert_bw.pack(side=LEFT, expand=True)

        self.btn_convert_log = Button(
            self.canvas_buttons, text="Log", width=20, command=self.toggle_convert_log
        )
        self.btn_convert_log.pack(side=LEFT, expand=True)

        self.scale_brightness = Scale(window, from_=0, to=100, orient=HORIZONTAL)
        self.scale_brightness.pack(anchor=CENTER, expand=True)

        self.scale_exposure = Scale(window, from_=0, to=100, orient=HORIZONTAL)
        self.scale_exposure.pack(anchor=CENTER, expand=True)

        self.canvas_figures = Canvas(window)
        self.canvas_figures.pack(anchor=CENTER, expand=True)

        # The pixel tracking figure
        self.pixel_tracker = PixelTracker(
            row=self.vid.height // 2,
            col=self.vid.width // 2,
            buffer_size=100,
            master=self.canvas_figures,
        )
        self.pixel_tracker.pack(side=LEFT, expand=True)

        # The zoom
        self.canvas_zoom = ZoomCanvas(
            self.zoom_patch_size, self.zoom_gain, self.canvas_figures,
        )
        self.canvas_zoom.pack(side=RIGHT, expand=True)

        self.delay = 15  # milliseconds
        self.update()

        # Populate the window
        window.title("Blinky Viewer")

        self.window.mainloop()

    def toggle_checksat(self):
        self.checksat = toggle(self.checksat)

    def toggle_convert_bw(self):
        self.convert_bw = toggle(self.convert_bw)

    def toggle_convert_log(self):
        self.convert_log = toggle(self.convert_log)

    def update(self):
        self.change_brightness()
        self.change_exposure()

        # Get a frame from the video source
        ret, frame = self.vid.get_frame()

        if ret:

            self.pixel_tracker.update(frame)

            if self.checksat:
                sat_pix = np.where(frame == 255)
                sat_col = (0, 255, 0)
                for col in range(3):
                    sat_pix[2][:] = col
                    frame[sat_pix] = sat_col[col]

            if self.convert_bw:
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)

            if self.convert_log:
                tmp = frame.astype(np.float32)
                tmp = np.log2(tmp + 1)
                frame = (tmp * 255 / np.max(tmp)).astype(np.uint8)

            self.photo = PIL.ImageTk.PhotoImage(image=PIL.Image.fromarray(frame))
            self.canvas.create_image(0, 0, image=self.photo, anchor=NW)

            # create zoomed-in vignette
            self.canvas_zoom.update(
                frame, self.pixel_tracker.row, self.pixel_tracker.col
            )

        self.window.after(self.delay, self.update)

    def change_brightness(self):
        scale_val = self.scale_brightness.get() / 100
        if self.vid_brightness != scale_val:
            self.vid.brightness = scale_val
            self.vid_brightness = scale_val

    def change_exposure(self):
        scale_val = self.scale_exposure.get() / 100
        if self.vid_exposure != scale_val:
            self.vid.exposure = scale_val
            self.vid_exposure = scale_val

    def mouse_callback(self, event):
        self.pixel_tracker.reset(event.y, event.x)


def start_viewer(video_source):
    BlinkyViewer(tkinter.Tk(), video_source)
