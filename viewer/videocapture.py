import cv2


class VideoCapture:
    def __init__(self, video_source=0):
        # Open the video source
        self.vid = cv2.VideoCapture(video_source)
        if not self.vid.isOpened():
            raise ValueError("Unable to open video source", video_source)

    @property
    def width(self):
        return int(self.vid.get(cv2.CAP_PROP_FRAME_WIDTH))

    @property
    def height(self):
        return int(self.vid.get(cv2.CAP_PROP_FRAME_HEIGHT))

    @property
    def brightness(self):
        return self.vid.get(cv2.CAP_PROP_BRIGHTNESS)

    @brightness.setter
    def brightness(self, value):
        self.vid.set(cv2.CAP_PROP_BRIGHTNESS, value)

    @property
    def exposure(self):
        return self.vid.get(cv2.CAP_PROP_EXPOSURE)

    @exposure.setter
    def exposure(self, value):
        self.vid.set(cv2.CAP_PROP_EXPOSURE, value)

    @property
    def auto_exposure(self):
        return self.vid.get(cv2.CAP_PROP_AUTO_EXPOSURE)

    @auto_exposure.setter
    def auto_exposure(self, value):
        self.vid.set(cv2.CAP_PROP_AUTO_EXPOSURE, value)

    # Release the video source when the object is destroyed
    def __del__(self):
        if self.vid.isOpened():
            self.vid.release()

    def get_frame(self):
        if self.vid.isOpened():
            ret, frame = self.vid.read()
            if ret:
                # Return a boolean success flag and the current frame converted to BGR
                return (ret, cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
            else:
                return (ret, None)
        else:
            return (ret, None)
