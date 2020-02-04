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

        self.vid = None
        self.start_video()

        # Keep track of the current frame
        self.current_frame = None

        # GEOMETRY #
        ############

        # Here we want to compute the size of the different boxes
        screen_w = self.window.winfo_screenwidth()
        screen_h = self.window.winfo_screenheight()
        vid_h, vid_w = self.vid.shape

        # set the size of the video canvas
        self.vid_can_w = int(screen_w * 0.75)
        self.vid_can_h = int(self.vid_can_w * (vid_h / vid_w))

        if self.vid_can_h > 0.9 * screen_h:
            self.vid_can_h = int(screen_h * 0.75)
            self.vid_can_w = int(self.vid_can_h * (vid_w / vid_h))

        self.window_w = int(1.33 * self.vid_can_w)

        self.zoom_wh = int(0.33 * self.vid_can_w)
        self.fig_w = self.zoom_wh
        self.fig_h = self.vid_can_h - self.zoom_wh
        self.plist_w = self.zoom_wh
        self.plist_h = int(0.9 * screen_h) - self.vid_can_h

        self.buttons_w = int(self.vid_can_w / 3)
        self.buttons_h = int(0.9 * screen_h) - self.vid_can_h

        self.console_w = int(self.vid_can_w / 3)
        self.console_h = int(0.9 * screen_h) - self.vid_can_h

        self.info_w = self.vid_can_w - self.buttons_w - self.console_w
        self.info_h = int(0.9 * screen_h) - self.vid_can_h

        self.window_h = self.vid_can_h + self.plist_h

        self.window_w = int(1.03 * self.window_w)
        self.window_h = int(1.03 * self.window_h)

        self.window.geometry(f"{self.window_w}x{self.window_h}+0+0")

        # THE LEFT PANEL #
        ##################

        # create canvas of the right size
        self.canvas = Canvas(
            self.window, width=self.vid_can_w, height=self.vid_can_h
        )
        self.canvas.grid(row=0, column=0, rowspan=2, columnspan=4)
        self.canvas.bind("<Button-1>", self.mouse_callback)

        # The control frame with all buttons
        self.frame_control = Frame(self.window)
        self.frame_control.grid(row=2, column=0, sticky=N)

        # The Buttons
        self.frame_control_buttons = Frame(self.frame_control)
        self.frame_control_buttons.grid(row=0, column=0)

        self.btn_checksat = Button(
            self.frame_control_buttons, text="Sat", width=10, command=self.toggle_checksat
        )
        self.btn_checksat.grid(row=0, column=0)

        self.btn_convert_bw = Button(
            self.frame_control_buttons, text="BW", width=10, command=self.toggle_convert_bw
        )
        self.btn_convert_bw.grid(row=1, column=0)

        self.btn_convert_log = Button(
            self.frame_control_buttons, text="Log", width=10, command=self.toggle_convert_log
        )
        self.btn_convert_log.grid(row=2, column=0)

        # The processing part
        self.frame_control_proc = Frame(self.window)
        self.frame_control_proc.grid(row=2, column=1, sticky="n")

        self.label_frame = Label(
            self.frame_control_proc, text=f"Record:"
        )
        self.label_frame.grid(row=0, column=0, sticky="nw")

        self.output_filename = DEFAULT_FILENAME
        self.label_file = Label(
            self.frame_control_proc, text=f"Output file:"
        )
        self.label_file.grid(row=1, column=0, sticky="nw")

        self.entry_filename = Entry(self.frame_control_proc, width=20)
        self.entry_filename.insert(0, self.output_filename)
        self.entry_filename.grid(row=1, column=1, sticky="nw")

        self.label_boxsize = Label(self.frame_control_proc, text="Box size:")
        self.label_boxsize.grid(row=2, column=0, sticky="nw")

        self.entry_boxsize = Entry(self.frame_control_proc, width=3)
        self.entry_boxsize.insert(0, "1")
        self.entry_boxsize.grid(row=2, column=1, sticky="nw")

        self.btn_process = Button(
            self.frame_control_proc, text=PROCESS_LABEL, width=10, command=self.process_callback
        )
        self.btn_process.grid(row=3, column=0)

        self.canvas_info = InfoBox({ "fps_video": 0, "fps_proc": 0 }, self.window, width=self.info_w // 2, height=self.info_h)
        self.canvas_info.grid(row=2, column=2, sticky="n")

        # The Console
        self.canvas_console = Canvas(self.window, width=self.console_w, height=self.console_h)
        self.canvas_console.grid(row=2, column=3, sticky="ne")
        self.console = ScrolledText(self.canvas_console, state=DISABLED, borderwidth=2, relief="solid", width=60, height=12)
        self.console.grid(row=0, column=0, sticky="ne")

        # THE RIGHT PANEL #
        ###################

        # The pixel tracking figure
        self.pixel_tracker = PixelTracker(
            row=self.vid.height // 2,
            col=self.vid.width // 2,
            width=self.fig_w,
            height=self.fig_h,
            buffer_size=100,
            master=self.window,
        )
        self.pixel_tracker.grid(row=0, column=4)

        # The zoom
        self.canvas_zoom = ZoomCanvas(
            self.zoom_patch_size,
            self.zoom_wh,
            self.vid.shape,
            self.window,
        )
        self.canvas_zoom.grid(row=1, column=4)
        self.canvas_zoom.bind("<Button-1>", self.zoom_callback)

        # Notify pixel tracker of initial zoom selection
        self.pixel_tracker.add(self.canvas_zoom.selected, label=PREVIEW_LABEL)

        # Buttons to add and remove pixels
        self.frame_pix_btn = Frame(self.window)
        self.frame_pix_btn.grid(row=2, column=4)

        self.btn_add_pixel = Button(
            self.frame_pix_btn,
            text="Add Pixel",
            width=10,
            command=self.add_pixel_callback,
        )
        self.btn_add_pixel.grid(row=0, column=0, sticky="nw")

        self.btn_drop_pixel = Button(
            self.frame_pix_btn,
            text="Drop Pixel",
            width=10,
            command=self.drop_pixel_callback,
        )
        self.btn_drop_pixel.grid(row=0, column=1, sticky="nw")

        # The selected pixels list
        self.pixel_list = PixelList(self.frame_pix_btn, width=self.plist_w // 2, height=self.plist_h)  #, list_width=40, list_height=10)
        self.pixel_list.grid(row=0, column=2, rowspan=2, sticky="nw")

        self.log("Welcome to BlinkyViewer")

        self.update()

        # Populate the window
        window.title("BlinkyViewer")

        self.window.mainloop()

    def start_video(self):
        # first stop the video if already running
        if self.vid is not None:
            self.vid.stop()

        if not self.industrial:
            self.vid = ThreadedVideoStream(self.video_source)
        else:
            if not icube_sdk_available:
                raise ValueError("The Driver for the DN3V camera is not available")

            self.vid = icube_sdk.ICubeCamera(self.video_source, 200)
            self.vid.start()

        print("Video FPS:", self.vid.fps)

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

        fps_proc = self.processor.fps if self.processor is not None else 0.
        self.canvas_info.update(fps_video=f"{self.vid.fps:6.2f}", fps_proc=f"{fps_proc:6.2f}")

        new_frame = None

        # Get a frame from the video source
        while self.vid.available:
            new_frame = np.array(self.vid.read(block=False), copy=False)

            if new_frame is None:
                break

            if self.processor is not None:
                self.processor.process(new_frame)

            self.pixel_tracker.push(new_frame)

        # If the video stream stopped, restart it
        if not self.vid.is_streaming:
            # if recording, stop
            if self.process_is_recording():
                self.process_callback()

            # restart video
            self.start_video()

            self.log("Looping the video")

        if new_frame is not None:
            self.current_frame = new_frame

        if self.current_frame is not None:

            # shorten
            frame = self.current_frame

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

    def process_is_recording(self):
        return self.btn_process.cget("text") == STOP_LABEL

    def process_is_not_recording(self):
        return self.btn_process.cget("text") == PROCESS_LABEL

    def process_callback(self):
        """ Called when pressing the "Process" button """

        if self.process_is_not_recording():

            bbox = self.entry_boxsize.get()
            pixel_list = self.pixel_list.get()
            self.output_filename = self.entry_filename.get()

            if self.output_filename == "":
                self.log("Please choose a filename")

            elif not bbox.isdigit():
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

        elif self.process_is_recording():

            self.log("Stop recording")

            self.btn_process.config(text=PROCESS_LABEL)

            if isinstance(self.processor, BoxCatcher):
                # stop the processor
                self.processor.stop()

                # save the result in blinky file
                new_file = BlinkyFile(
                    self.processor.pixels, self.processor.data, self.vid.fps
                )
                new_file.dump(self.output_filename)

                # Replace by the simple speed meter
                self.processor = ReadSpeedMonitor(monitor=True)

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

def viewer_main():
    import argparse

    parser = argparse.ArgumentParser(
        description="A video viewer to help setup and debug Blinkies"
    )
    parser.add_argument(
        "-v",
        "--video_source",
        type=str,
        default=0,
        help="The video feed number (default 0)",
    )
    parser.add_argument(
        "-i",
        "--industrial",
        action="store_true"
    )
    args = parser.parse_args()

    try:
        video_source = int(args.video_source)
    except ValueError:
        video_source = args.video_source

    start_viewer(video_source, industrial=args.industrial)


if __name__ == "__main__":
    viewer_main()
