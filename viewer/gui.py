import tkinter
from tkinter import *
from tkinter.scrolledtext import ScrolledText
from tkinter.filedialog import asksaveasfilename

import PIL
import PIL.ImageTk
import numpy as np
import cv2

try:
    import icube_sdk
    icube_sdk_available = True
except ImportError:
    icube_sdk_available = False

from .gui_utils import *
from .videocapture import VideoCapture
from .video import ThreadedVideoStream
from .processors import ReadSpeedMonitor, BoxCatcher
from .utils import pixel_to_str
from .io import BlinkyFile


def toggle(x: bool):
    return x ^ True


# Now come the GUI part
class BlinkyViewer(object):
    def __init__(self, window, video_source=0, industrial=False):
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

        # Choose the video source
        self.industrial = industrial
        if not self.industrial:
            self.vid = ThreadedVideoStream(self.video_source)
            print("Video FPS:", self.vid.fps)
        else:
            if not icube_sdk_available:
                raise ValueError("The Driver for the DN3V camera is not available")

            self.vid = icube_sdk.ICubeCamera(self.video_source, 200)
            self.vid.start()

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

        if self.vid_can_h > 0.9 * screen_h:
            self.vid_can_h = int(screen_h * 0.80)
            self.vid_can_w = int(self.vid_can_h * (vid_w / vid_h))

        self.zoom_wh = int(screen_w * 0.2)
        self.fig_w = self.zoom_wh
        self.fig_h = 0.7 * (self.vid_can_h - self.zoom_wh)
        self.plist_w = self.zoom_wh
        self.plist_h = self.vid_can_h - self.zoom_wh - self.fig_h

        self.buttons_w = int(self.vid_can_w / 3)
        self.buttons_h = screen_h - self.vid_can_h

        self.console_w = int(self.vid_can_w / 3)
        self.console_h = screen_h - self.vid_can_h

        self.info_w = self.vid_can_w - self.buttons_w - self.console_w
        self.info_h = screen_h - self.vid_can_h

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

        # Put the widegets from the bottom in a separate canvas
        self.left_bottom_canvas = Canvas(self.left_canvas, width=self.vid_can_w, height=screen_h - self.vid_can_h)
        self.left_bottom_canvas.pack(anchor=CENTER)

        # The Buttons
        self.canvas_buttons = Canvas(self.left_bottom_canvas, width=self.buttons_w)
        self.canvas_buttons.pack(side=LEFT, expand=True)
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
        self.canvas_proc = Canvas(self.left_bottom_canvas, width=self.buttons_w)
        self.canvas_proc.pack(anchor=CENTER, expand=True)

        self.output_filename = DEFAULT_FILENAME
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
            self.canvas_proc, text=PROCESS_LABEL, width=20, command=self.process_callback
        )
        self.btn_process.pack(side=LEFT, expand=True)

        self.canvas_info = InfoBox({ "fps_video": 0, "fps_proc": 0 }, self.left_bottom_canvas, width=self.info_w, height=self.info_h)
        self.canvas_info.pack(side=LEFT, expand=True)

        # The Console
        self.canvas_console = Canvas(self.left_bottom_canvas, width=self.console_w, height=self.console_h)
        self.canvas_console.pack_propagate(False)
        self.canvas_console.pack(side=LEFT)
        self.console = ScrolledText(self.canvas_console, state=DISABLED)
        self.console.pack(anchor=CENTER, expand=True)

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
        self.pixel_list = PixelList(self.right_canvas, width=self.plist_w, height=self.plist_h)  #, list_width=40, list_height=10)
        self.pixel_list.pack(anchor=CENTER, expand=True)

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

        fps_proc = self.processor.fps if self.processor is not None else None
        self.canvas_info.update(fps_video=self.vid.fps, fps_proc=fps_proc)

        frame = None

        # Get a frame from the video source
        while self.vid.available:
            frame = np.array(self.vid.read(block=False), copy=False)

            if frame is None:
                break

            if self.processor is not None:
                self.processor.process(frame)

            self.pixel_tracker.push(frame)

        if frame is not None:

            self.pixel_tracker.update()

            if frame.ndim == 2:
                frame = cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)

            elif self.convert_bw:
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
        tmp_filename = asksaveasfilename(defaultextension=".blinky", initialdir=".")
        if tmp_filename != "":
            self.output_filename = tmp_filename
            self.label_file.config(text=f"Output file: {self.output_filename}")

    def process_callback(self):
        """ Called when pressing the "Process" button """

        if self.btn_process.cget("text") == PROCESS_LABEL:

            bbox = self.entry_boxsize.get()
            pixel_list = self.pixel_list.get()

            if not bbox.isdigit():
                self.log("The box size should be a number")

            elif len(pixel_list) == 0:
                self.log("No pixel has been selected for recording.")

            else:

                bbox = int(bbox)

                self.log("Start recording")

                # If the video is from a file, we restart
                if not isinstance(self.video_source, int):
                    self.vid.stop()

                if self.processor is not None:
                    self.processor.stop()

                self.processor = BoxCatcher(
                    pixel_list, [bbox, bbox], monitor=True
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


def start_viewer(video_source, industrial=False):
    BlinkyViewer(Tk(), video_source, industrial=industrial)
