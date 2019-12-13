import argparse
from .gui import start_viewer

if __name__ == "__main__":
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
    args = parser.parse_args()

    try:
        video_source = int(args.video_source)
    except ValueError:
        video_source = args.video_source

    start_viewer(args.video_source)
