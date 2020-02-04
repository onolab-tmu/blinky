# Copyright 2020 Robin Scheibler
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
import numpy as np
from tkinter import *
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

import PIL
import PIL.ImageTk
import numpy as np
import cv2


PREVIEW_LABEL = "Preview"
PROCESS_LABEL = "Start"
STOP_LABEL = "Stop"
DEFAULT_FILENAME = "myfile.blinky"


def pixel_to_str(pixel):
    return f"({pixel[0]}, {pixel[1]})"


class InfoBox(Frame):
    """
    Parameters
    ----------
    content: dict
        A dictionary of key/val pairs used for labels/content
    *args:
        Positional arguments of TkInter Canvas
    **kwargs:
        Keyword arguments of TkInter Canvas
    """
    def __init__(self, content, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.grid_propagate(False)

        info_label = Label(self, text="Info:")
        info_label.grid(row=0, column=0, columnspan=2, sticky="w")

        self.fields = {}

        for row, (lbl, value) in enumerate(content.items()):
            new = {
                "label": Label(self, text=lbl + ": "),
                "value": Label(self, text=str(value)),
            }
            new["label"].grid(row=row + 1, column=0, sticky="w")
            new["value"].grid(row=row + 1, column=1, sticky="w")
            self.fields[lbl] = new

    def update(self, **content):
        for lbl, value in content.items():
            if lbl in self.fields:
                self.fields[lbl]["value"].config(text=str(value))

class PixelList(Frame):
    def __init__(self, window, *args, **kwargs):

        super().__init__(window, *args, **kwargs)
        self.grid_propagate(False)
        self.pixels = {}

        self.list = Listbox(self, width=25, height=12)  # , width=list_width, height=list_height)
        self.list.grid(row=0, column=0, sticky="nw")

    def add(self, pixel):
        """ Add a pixel to the list """
        label = pixel_to_str(pixel)
        if label not in self.pixels:
            self.pixels[label] = pixel
            self.list.insert(END, label)

    def get(self):
        """ Return the list of pixels """
        return list(self.pixels.values())

    @property
    def curselection(self):
        """ Return label of selected pixel """
        return [self.list.get(p) for p in self.list.curselection()]

    def curdelete(self):
        """ Delete currently selected item """
        cur_selected = self.list.curselection()
        for p in cur_selected:
            p_lbl = self.list.get(p)  # get label
            self.pixels.pop(p_lbl)  # remove from pixel list
            self.list.delete(p)  # delete from listbox


class ZoomCanvas(Canvas):
    def __init__(self, patch_size, win_size, frame_shape, *args, **kwargs):
        self.win_size = win_size
        self.patch_size = patch_size
        self.pixel_size = self.win_size / self.patch_size
        self.frame_shape = frame_shape

        self._n_offset = (self.patch_size - 1) // 2
        self.select_row = self._n_offset + 1
        self.select_col = self._n_offset + 1

        # initialize origin in upper left corner
        self.origin = (self._n_offset, self._n_offset)
        self.selected_local = (0, 0)

        self._vignette = None

        super().__init__(width=self.win_size, height=self.win_size, *args, **kwargs)

        self.image = None

    @property
    def selected(self):
        return (
            self.origin[0] + self.selected_local[0],
            self.origin[1] + self.selected_local[1],
        )

    def on_click(self, event):

        c, r = event.x, event.y

        self.selected_local = (
            round(c / self.pixel_size - 0.5) - self._n_offset,
            round(r / self.pixel_size - 0.5) - self._n_offset,
        )

    def set_origin(self, pixel):

        self.origin = pixel
        self._sanitize_origin()

        # reset local selection upon re-framing
        self.selected_local = (0, 0)

    def _sanitize_origin(self):

        tmp = list(self.origin)

        # Force vignette boundaries to be in the frame
        for i in [0, 1]:
            if tmp[i] - self._n_offset < 0:
                tmp[i] = self._n_offset
            elif tmp[i] - self._n_offset + self.patch_size > self.frame_shape[1 - i]:
                tmp[i] = self.frame_shape[1 - i] - self.patch_size + self._n_offset

        self.origin = tuple(tmp)

    def update(self, frame):

        col = self.origin[0] - self._n_offset
        row = self.origin[1] - self._n_offset

        cut_out = frame[
            row : row + self.patch_size, col : col + self.patch_size,
        ].copy()

        # Resize the patch
        z = cv2.resize(
            cut_out, (self.win_size, self.win_size), interpolation=cv2.INTER_NEAREST
        )

        # Draw a red rectangle around the selected pixel
        p_s = (
            round((self.selected_local[0] + self._n_offset) * self.pixel_size),
            round((self.selected_local[1] + self._n_offset) * self.pixel_size),
        )
        p_e = (p_s[0] + round(self.pixel_size), p_s[1] + round(self.pixel_size))
        z = cv2.rectangle(z, p_s, p_e, (255.0, 0.0, 0.0), 1)

        self._vignette = PIL.ImageTk.PhotoImage(image=PIL.Image.fromarray(z))
        self.create_image(0, 0, image=self._vignette, anchor="nw", tags="zoom_image")

    def mark(self, frame, conv_func=None):
        """ Mark location of zoom in original frame """
        c1 = self.origin[0] - self._n_offset
        r1 = self.origin[1] - self._n_offset
        c2 = c1 + self.patch_size
        r2 = r1 + self.patch_size
        if conv_func is not None:
            c1, r1 = conv_func(c1, r1)
            c2, r2 = conv_func(c2, r2)
        frame = cv2.rectangle(frame, (c1, r1), (c2, r2), (255.0, 0.0, 0.0), 1,)

        return frame


class PixelTracker(object):
    def __init__(
        self,
        row=0,
        col=0,
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
        self.fig.set_tight_layout(True)
        self.ax = self.fig.add_subplot(111)
        self.plt_canvas = FigureCanvasTkAgg(self.fig, master=master)
        self.plt_canvas.draw()

        # Attributes for the pixel tracker
        self.buffer_size = buffer_size
        self.reset(row, col)

        self.pixels = {}

    def add(self, pixel, label=None):
        if label is None:
            label = pixel_to_str(pixel)

        # If this is not a preview and the label is already
        # in the list, we abort
        if label != PREVIEW_LABEL and label in self.pixels:
            return

        # Add the pixel
        self.pixels[label] = {
            "loc": pixel,
            "values": [0 for i in range(self.buffer_size)],
        }

    def remove(self, pixel_str):
        self.pixels.pop(pixel_str)

    def grid(self, *args, **kwargs):
        self.plt_canvas.get_tk_widget().grid(*args, **kwargs)

    def reset(self, row, col):
        self.row = row
        self.col = col
        self.values = [0 for i in range(self.buffer_size)]

    def push(self, frame):

        for lbl, pxl in self.pixels.items():

            col, row = pxl["loc"]

            # Make the value grayscale if it isn't already
            if frame.ndim == 3:
                value = cv2.cvtColor(
                    frame[row, col, None, None, :], cv2.COLOR_RGB2GRAY
                )[0, 0]
            elif frame.ndim == 2:
                value = frame[row, col]
            else:
                raise ValueError("Bad input")

            # add to the buffer
            pxl["values"].pop(0)
            pxl["values"].append(value)

    def update(self):

        # update the graph
        self.ax.clear()

        for lbl, pxl in self.pixels.items():
            self.ax.plot(
                np.arange(-len(pxl["values"]) + 1, 1), pxl["values"], label=lbl
            )

        if len(self.pixels) > 0:
            self.ax.legend(loc="lower left")
        self.ax.set_ylim([0, 255])
        self.ax.set_xticks([])
        self.ax.set_yticks([])
        self.fig.tight_layout(pad=0)
        self.plt_canvas.draw()

    def mark(self, frame, conv_func):
        """ Mark the locations of pixels with circles on a frame """
        mark_radius = 4
        mark_color_preview = (0, 0, 255)
        mark_color_selected = (255, 0, 0)
        for lbl, p in self.pixels.items():
            if lbl == PREVIEW_LABEL:
                c = mark_color_preview
            else:
                c = mark_color_selected
            if conv_func is not None:
                loc = conv_func(*p["loc"])
            else:
                loc = p["loc"]
            frame = cv2.circle(frame, loc, mark_radius, c, 2)

        return frame

