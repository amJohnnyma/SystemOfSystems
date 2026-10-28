
'''
python3 generate_items.py \
  --start 100 \
  --count 50 \
  --sprite-sheet res://game/atlas/items.png \
  --output items.json


python3 generate_items.py \
  --start 1 \
  --count 100 \
  --sprite-sheet res://game/atlas/tools.png \
  --frame-width 66 \
  --frame-height 64 \
  --output tools.json
'''
#!/usr/bin/env python3
"""Generate a JSON item registry with default terrain-modifying items."""

import argparse
import json
from pathlib import Path


def generate_items(start_id: int, count: int, sprite_path: str,
                   frame_width: int = 32, frame_height: int = 32) -> dict:
    sprite_name = Path(sprite_path).stem
    if not sprite_name:
        raise ValueError("Sprite sheet filename cannot be empty")
    if count < 1:
        raise ValueError("count must be at least 1")
    if start_id < 0:
        raise ValueError("start_id cannot be negative")
    if frame_width < 1 or frame_height < 1:
        raise ValueError("frame dimensions must be positive")

    items = []
    for icon_frame in range(count):
        item_id = start_id + icon_frame
        items.append({
            "iID": item_id,
            "name": f"item{item_id}",
            "description": f"item{item_id}description",
            "category": "Tool",
            "max_stack": 1,
            "can_go_hotbar": True,
            "max_durability": 250,
            "weight": 0.0,
            "icon": "",
            "world_sprite": "",
            "world_size": [1.0, 1.0],
            "animations": {},
            "actions": [
                {
                    "type": "ModifyTerrain",
                    "range": 4.0,
                    "cooldown": 0.35,
                    "density_change": -32.0,
                    "radius": 1.5,
                    "durability_cost": 1,
                    "events": [
                        {"type": "ApplyAction", "normalized_time": 0.5}
                    ],
                }
            ],
            "sprite_sheet": sprite_name,
            "icon_frame": icon_frame,
        })

    return {
        "sprite_sheets": {
            sprite_name: {
                "path": sprite_path,
                "frame_size": [frame_width, frame_height],
            }
        },
        "items": items,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--start", type=int, default=1,
                        help="First item ID (default: 1)")
    parser.add_argument("--count", type=int, required=True,
                        help="Number of items to generate")
    parser.add_argument("--sprite-sheet", default="res://game/atlas/items.png",
                        help="Godot sprite sheet resource path")
    parser.add_argument("--frame-width", type=int, default=32)
    parser.add_argument("--frame-height", type=int, default=32)
    parser.add_argument("--output", default="items.json",
                        help="Output JSON filename (default: items.json)")
    args = parser.parse_args()

    try:
        data = generate_items(
            args.start, args.count, args.sprite_sheet,
            args.frame_width, args.frame_height,
        )
    except ValueError as error:
        parser.error(str(error))

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {args.count} items (IDs {args.start}–{args.start + args.count - 1}) in {output_path}")


if __name__ == "__main__":
    main()
