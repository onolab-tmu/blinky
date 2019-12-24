import tkinter
from tkinter import (
    Button,
    Canvas,
    Listbox,
    Scale,
    Label,
    Entry,
    NW,
    CENTER,
    NE,
    HORIZONTAL,
    N,
    E,
    W,
    RIGHT,
    LEFT,
    BOTTOM,
    TOP,
    END,
    DISABLED,
)
from tkinter.scrolledtext import ScrolledText
from tkinter.filedialog import askopenfilename, asksaveasfilename
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

import PIL
import PIL.ImageTk
import numpy as np
import cv2

from .videocapture import VideoCapture
from .video import ThreadedVideoStream
from .processors import ReadSpeedMonitor, BoxCatcher
from .utils import pixel_to_str
from .io import BlinkyFile


PREVIEW_LABEL = "Preview"
PROCESS_LABEL = "Process"
STOP_LABEL = "Stop"


def toggle(x: bool):
    return x ^ True


def pixel_to_str(pixel):
    return f"({pixel[0]}, {pixel[1]})"


class PixelList(Canvas):
    def __init__(self, window, list_width=20, list_height=10, *args, **kwargs):

        super().__init__(window, *args, **kwargs)
        self.pixels = {}

        self.list = Listbox(self, width=list_width, height=list_height)
        self.list.pack()

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

    def pack(self, *args, **kwargs):
        self.plt_canvas.get_tk_widget().pack(*args, **kwargs)

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


# Now come the GUI part
class BlinkyViewer(object):
    def __init__(self, window, video_source=0):
        self.window = window
        self.window.protocol("WM_DELETE_WINDOW", self.on_closing_callback)
        # overload "Exit" function for Menu file -> quit
        self.window.createcommand("exit", self.on_closing_callback)

        # window update delay
        self.delay = 33  # milliseconds

        # control
        self.checksat = False
        self.convert_bw = False
        self.convert_log = False

        # zoom size
        self.zoom_patch_size = 31

        # open video feed
        self.video_source = video_source
        self.processor = ReadSpeedMonitor(monitor=True)
        self.vid = ThreadedVideoStream(self.video_source)
        print("Video FPS:", self.vid.fps)

        self.vid_brightness = self.vid.brightness
        self.vid_exposure = self.vid.exposure

        # GEOMETRY #
        ############

        # Here we want to compute the size of the different boxes
        screen_w = self.window.winfo_screenwidth()
        screen_h = self.window.winfo_screenheight()
        vid_h, vid_w = self.vid.shape
        self.window.geometry(f"{screen_w}x{screen_h}+0+0")

        # set the size of the video canvas
        self.vid_can_w = int(screen_w * 0.80)
        self.vid_can_h = int(self.vid_can_w * (vid_h / vid_w))

        self.zoom_wh = int(screen_w * 0.2)
        self.fig_w = self.zoom_wh
        self.fig_h = 0.7 * (self.vid_can_h - self.zoom_wh)

        # THE LEFT PANEL #
        ##################

        self.left_canvas = Canvas(window)
        self.left_canvas.pack(side=LEFT, expand=True)

        # create canvas of the right size
        self.canvas = Canvas(
            self.left_canvas, width=self.vid_can_w, height=self.vid_can_h
        )
        self.canvas.pack(anchor=N, expand=True)
        self.canvas.bind("<Button-1>", self.mouse_callback)

        # The Buttons
        self.canvas_buttons = Canvas(self.left_canvas)
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

        # The processing part
        self.canvas_proc = Canvas(self.left_canvas)
        self.canvas_proc.pack(anchor=CENTER, expand=True)

        self.output_filename = "blinky_viewer_output.txt"
        self.label_file = Label(
            self.canvas_proc, text=f"Output file: {self.output_filename}"
        )
        self.label_file.pack(side=LEFT, expand=True)

        self.btn_file_dialog = Button(
            self.canvas_proc, text="...", width=3, command=self.choose_file_callback
        )
        self.btn_file_dialog.pack(side=LEFT, expand=True)

        self.label_boxsize = Label(self.canvas_proc, text="Box size:")
        self.label_boxsize.pack(side=LEFT, expand=True)

        self.entry_boxsize = Entry(self.canvas_proc, width=10)
        self.entry_boxsize.insert(0, "1")
        self.entry_boxsize.pack(side=LEFT, expand=True)

        self.btn_process = Button(
            self.canvas_proc, text="Process", width=20, command=self.process_callback
        )
        self.btn_process.pack(side=LEFT, expand=True)

        # THE RIGHT PANEL #
        ###################

        self.right_canvas = Canvas(window)
        self.right_canvas.pack(side=LEFT, expand=True)

        # The pixel tracking figure
        self.pixel_tracker = PixelTracker(
            row=self.vid.height // 2,
            col=self.vid.width // 2,
            width=self.fig_w,
            height=self.fig_h,
            buffer_size=100,
            master=self.right_canvas,
        )
        self.pixel_tracker.pack(anchor=CENTER, expand=True)

        # The zoom
        self.canvas_zoom = ZoomCanvas(
            self.zoom_patch_size,
            self.zoom_wh,
            self.vid.shape,
            self.right_canvas,
        )
        self.canvas_zoom.pack(anchor=CENTER, expand=True)
        self.canvas_zoom.bind("<Button-1>", self.zoom_callback)

        # Notify pixel tracker of initial zoom selection
        self.pixel_tracker.add(self.canvas_zoom.selected, label=PREVIEW_LABEL)

        # Buttons to add and remove pixels
        self.canvas_pix_btn = Canvas(self.right_canvas)
        self.canvas_pix_btn.pack(anchor=CENTER, expand=True)

        self.btn_add_pixel = Button(
            self.canvas_pix_btn,
            text="Add Pixel",
            width=20,
            command=self.add_pixel_callback,
        )
        self.btn_add_pixel.pack(side=LEFT, expand=True)

        self.btn_drop_pixel = Button(
            self.canvas_pix_btn,
            text="Drop Pixel",
            width=20,
            command=self.drop_pixel_callback,
        )
        self.btn_drop_pixel.pack(side=LEFT, expand=True)

        # The selected pixels list
        self.pixel_list = PixelList(self.right_canvas, list_width=40, list_height=10)
        self.pixel_list.pack(anchor=CENTER, expand=True)

        # The Console
        self.console = ScrolledText(
            self.right_canvas, width=50, height=10, state=DISABLED
        )
        self.console.pack(anchor=CENTER, expand=True)

        self.log("Welcome to BlinkyViewer")

        self.update()

        # Populate the window
        window.title("BlinkyViewer")

        self.window.mainloop()

    def log(self, message):
        self.console.configure(state="normal")
        self.console.insert("end", "\n" + message)
        self.console.configure(state="disabled")
        self.console.see("end")

    def coord_canvas_to_video(self, col, row):
        """ Transforms from canvas to video coordinate """
        c = int((col / self.vid_can_w) * self.vid.width)
        r = int((row / self.vid_can_h) * self.vid.height)
        return c, r

    def coord_video_to_canvas(self, col, row):
        """ Transforms from video to canvas coordinate """
        c = int((col / self.vid.width) * self.vid_can_w)
        r = int((row / self.vid.height) * self.vid_can_h)
        return c, r

    def update(self):

        frame = None

        # Get a frame from the video source
        while self.vid.available:
            frame = self.vid.read(block=False)

            if frame is None:
                break

            if self.processor is not None:
                self.processor.process(frame)

            self.pixel_tracker.push(frame)

        if frame is not None:

            self.pixel_tracker.update()

            if self.convert_bw:
                # convert to black and white
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
                # but convert back to color so that we
                # an add visual help in color
                frame = cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)

            if self.checksat:
                sat_pix = np.where(frame == 255)
                sat_col = (0, 255, 0)
                for col in range(3):
                    sat_pix[2][:] = col
                    frame[sat_pix] = sat_col[col]

            if self.convert_log:
                tmp = frame.astype(np.float32)
                tmp = np.log2(tmp + 1)
                frame = (tmp * 255 / np.max(tmp)).astype(np.uint8)

            # create zoomed-in vignette
            self.canvas_zoom.update(frame)

            # Display the video frame
            frame = cv2.resize(
                frame, (self.vid_can_w, self.vid_can_h), interpolation=cv2.INTER_NEAREST
            )

            # indicate the location of selected pixels
            frame = self.pixel_tracker.mark(frame, conv_func=self.coord_video_to_canvas)
            frame = self.canvas_zoom.mark(frame, conv_func=self.coord_video_to_canvas)

            self.photo = PIL.ImageTk.PhotoImage(image=PIL.Image.fromarray(frame))
            self.canvas.create_image(0, 0, image=self.photo, anchor=NW)

        self.window.after(self.delay, self.update)

    def toggle_checksat(self):
        self.checksat = toggle(self.checksat)

    def toggle_convert_bw(self):
        self.convert_bw = toggle(self.convert_bw)

    def toggle_convert_log(self):
        self.convert_log = toggle(self.convert_log)

    def add_pixel_callback(self):
        self.pixel_list.add(self.canvas_zoom.selected)
        self.pixel_tracker.add(self.canvas_zoom.selected)
        self.log(f"Selected pixel at {pixel_to_str(self.canvas_zoom.selected)}")

    def drop_pixel_callback(self):
        for plbl in self.pixel_list.curselection:
            self.pixel_tracker.remove(plbl)
        self.pixel_list.curdelete()
        self.log(f"Dropped pixel at {pixel_to_str(self.canvas_zoom.selected)}")

    def choose_file_callback(self):
        """ Called when pressing on the choose file button """
        tmp_filename = asksaveasfilename()
        if tmp_filename != "":
            self.output_filename = tmp_filename
            self.label_file.config(text=f"Output file: {self.output_filename}")

    def process_callback(self):
        """ Called when pressing the "Process" button """

        if self.btn_process.cget("text") == PROCESS_LABEL:

            bbox = self.entry_boxsize.get()

            if not bbox.isdigit():
                self.log("The box size should be a number")

            else:

                bbox = int(bbox)

                self.log("Start recording")

                # If the video is from a file, we restart
                if not isinstance(self.video_source, int):
                    self.vid.stop()

                if self.processor is not None:
                    self.processor.stop()

                self.processor = BoxCatcher(
                    self.pixel_list.get(), [bbox, bbox], monitor=True
                )

                # If the video is from a file, we restart
                if not isinstance(self.video_source, int):
                    self.vid = ThreadedVideoStream(self.video_source)

                self.btn_process.config(text=STOP_LABEL)

        elif self.btn_process.cget("text") == STOP_LABEL:

            self.log("Stop recording")

            self.btn_process.config(text=PROCESS_LABEL)

            if isinstance(self.processor, BoxCatcher):
                self.processor.stop()
                new_file = BlinkyFile(
                    self.processor.pixels, self.processor.data, self.vid.fps
                )
                new_file.dump(self.output_filename)

        else:
            raise ValueError("Invalid button state")

    def mouse_callback(self, event):
        """ Called when clicking on the main camera display """
        col, row = self.coord_canvas_to_video(event.x, event.y)
        frame_shape = self.vid.shape
        if col >= frame_shape[1]:
            col = frame_shape[1] - 1
        if row >= frame_shape[0]:
            row = frame_shape[0] - 1

        self.canvas_zoom.set_origin((col, row))
        self.pixel_tracker.add(self.canvas_zoom.selected, label=PREVIEW_LABEL)

    def zoom_callback(self, event):
        """ Called when clicking on the zoomed out vignette """
        self.canvas_zoom.on_click(event)
        self.pixel_tracker.add(
            self.canvas_zoom.selected, label=PREVIEW_LABEL,
        )

    def on_closing_callback(self):
        self.log("Goodbye!")
        self.vid.stop()
        if self.processor is not None:
            self.processor.stop()

        self.window.destroy()


def start_viewer(video_source):
    BlinkyViewer(tkinter.Tk(), video_source)
