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
    W,
    RIGHT,
    LEFT,
    TOP,
)
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

import PIL
import PIL.ImageTk
import numpy as np
import cv2

from .videocapture import VideoCapture
from .video import ThreadedVideoStream


def toggle(x: bool):
    return x ^ True


class ZoomCanvas(Canvas):
    def __init__(self, patch_size, zoom_gain, *args, **kwargs):
        self.win_size = patch_size * zoom_gain
        self.patch_size = patch_size
        self.pixel_size = self.win_size / self.patch_size
        self.gain = zoom_gain

        self._n_offset = (self.patch_size - 1) // 2
        self.select_row = self._n_offset + 1
        self.select_col = self._n_offset + 1

        self._vignette = None

        super().__init__(width=self.win_size, height=self.win_size, *args, **kwargs)

        self.image = None

    def on_click(self, event):

        c, r = event.x, event.y

        self.select_row = round(r / self.pixel_size - 0.5)
        self.select_col = round(c / self.pixel_size - 0.5)

    def update(self, frame, row, col):

        row -= self._n_offset
        col -= self._n_offset

        # Force vignette boundaries to be in the frame
        if row < 0:
            row = 0
        elif row + self.patch_size > frame.shape[0]:
            row = frame.shape[0] - self.patch_size

        if col < 0:
            col = 0
        elif col + self.patch_size > frame.shape[1]:
            col = frame.shape[1] - self.patch_size

        cut_out = frame[
            row : row + self.patch_size, col : col + self.patch_size,
        ].copy()

        # Resize the patch
        z = cv2.resize(
            cut_out, (self.win_size, self.win_size), interpolation=cv2.INTER_NEAREST
        )

        # Draw a red rectangle around the selected pixel
        p_s = (
            round(self._n_offset * self.pixel_size),
            round(self._n_offset * self.pixel_size),
        )
        p_e = (p_s[0] + round(self.pixel_size), p_s[1] + round(self.pixel_size))
        z = cv2.rectangle(z, p_s, p_e, (255.0, 0.0, 0.0), 1)

        self._vignette = PIL.ImageTk.PhotoImage(image=PIL.Image.fromarray(z))
        self.create_image(0, 0, image=self._vignette, anchor="nw", tags="zoom_image")


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

        self.width = width
        self.height = height
        self.dpi = dpi

        self.fig = Figure(
            figsize=(self.width / self.dpi, self.height / self.dpi), dpi=self.dpi
        )
        self.ax = self.fig.add_subplot(111)
        self.plt_canvas = FigureCanvasTkAgg(self.fig, master=master)
        self.plt_canvas.draw()

        # Attributes for the pixel tracker
        self.buffer_size = buffer_size
        self.reset(row, col)

    def pack(self, *args, **kwargs):
        self.plt_canvas.get_tk_widget().pack(*args, **kwargs)

    def reset(self, row, col):
        self.row = row
        self.col = col
        self.values = [0 for i in range(self.buffer_size)]

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

        # add to the buffer
        self.values.pop(0)
        self.values.append(value)

        # update the graph
        self.ax.clear()
        self.ax.plot(np.arange(-len(self.values) + 1, 1), self.values)
        self.ax.set_title(f"Selected pixel: col={self.col} row={self.row}")
        self.ax.set_ylim([0, 255])
        self.plt_canvas.draw()


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

        self.top_canvas = Canvas(window)
        self.top_canvas.pack(expand=True)

        # create canvas of the right size
        self.canvas = Canvas(
            self.top_canvas, width=self.vid.width, height=self.vid.height
        )
        self.canvas.pack(side=LEFT, expand=True)
        self.canvas.bind("<Button-1>", self.mouse_callback)

        self.canvas_figures = Canvas(self.top_canvas)
        self.canvas_figures.pack(side=LEFT, expand=True)

        # The pixel tracking figure
        self.pixel_tracker = PixelTracker(
            row=self.vid.height // 2,
            col=self.vid.width // 2,
            buffer_size=100,
            master=self.canvas_figures,
        )
        self.pixel_tracker.pack(side=TOP, expand=True)

        # The zoom
        self.canvas_zoom = ZoomCanvas(
            self.zoom_patch_size, self.zoom_gain, self.canvas_figures,
        )
        self.canvas_zoom.pack(side=TOP, expand=True)
        self.canvas_zoom.bind("<Button-1>", self.zoom_callback)

        # The Buttons
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

    def mouse_callback(self, event):
        self.pixel_tracker.reset(event.y, event.x)

    def zoom_callback(self, event):
        self.canvas_zoom.on_click(event)
        new_row = (
            self.pixel_tracker.row
            + self.canvas_zoom.select_row
            - self.canvas_zoom._n_offset
        )
        new_col = (
            self.pixel_tracker.col
            + self.canvas_zoom.select_col
            - self.canvas_zoom._n_offset
        )
        self.pixel_tracker.reset(new_row, new_col)


def start_viewer(video_source):
    BlinkyViewer(tkinter.Tk(), video_source)
