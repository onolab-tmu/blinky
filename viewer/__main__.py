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
    parser.add_argument(
        "-i",
        "--industrial",
        action="store_true"
    )
    args = parser.parse_args()

    print(int(args.video_source))

    try:
        video_source = int(args.video_source)
    except ValueError:
        video_source = args.video_source

    start_viewer(video_source, industrial=args.industrial)
