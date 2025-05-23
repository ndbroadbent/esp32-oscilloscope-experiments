#!/usr/bin/env python3
"""
SVG to points converter for oscilloscope display.

This script properly extracts path data from an SVG file and converts it to a series of 
points that can be directly rendered on an oscilloscope. It uses svgpathtools for accurate
SVG path parsing and discretization.

Usage:
  python3 svg_to_points.py input.svg output.h [options]

Arguments:
  input.svg       Input SVG file
  output.h        Output header file
  --name          Variable name prefix for the generated code (default: "svg")
  --scale         Scale factor for the points (default: 255.0)
  --points-per-segment  Number of points per path segment (default: 50)
  --debug-svg     Output a debug SVG file showing the path segments (default: None)
"""

import argparse
import os
import math
import sys
from xml.dom import minidom
import re
try:
    from svgpathtools import Path, Line, QuadraticBezier, CubicBezier, Arc, parse_path
except ImportError:
    print("Error: svgpathtools library not found.")
    print("Please install it using: pip install svgpathtools")
    sys.exit(1)

def extract_paths_from_svg(svg_file):
    """
    Extract path data from an SVG file.
    Returns a list of parsed path objects and their attributes.
    """
    try:
        doc = minidom.parse(svg_file)
    except Exception as e:
        print(f"Error parsing SVG file: {e}")
        sys.exit(1)
        
    path_elements = doc.getElementsByTagName('path')
    
    if not path_elements:
        print(f"No path elements found in {svg_file}")
        sys.exit(1)
    
    parsed_paths = []
    
    # Extract path data from each path element
    for i, path_element in enumerate(path_elements):
        path_data = path_element.getAttribute('d')
        
        # Skip empty paths or paths with display="none"
        if not path_data or path_element.getAttribute('display') == 'none':
            continue
        
        # Get ID or generate one
        path_id = path_element.getAttribute('id') or f"path_{i}"
        
        try:
            # Parse the path data using svgpathtools
            path = parse_path(path_data)
            
            # Skip empty paths
            if not path:
                continue
                
            parsed_paths.append({
                'id': path_id,
                'path': path
            })
            
        except Exception as e:
            print(f"Error parsing path {path_id}: {e}")
            continue
    
    return parsed_paths

def path_to_discrete_points(path, points_per_segment=50):
    """
    Convert a path to a list of discrete points.
    Uses adaptive sampling based on segment length.
    Returns a list of path segments to properly handle 'move to' commands.
    """
    path_segments = []
    current_segment = []
    last_end_point = None
    
    # Handle special case for empty path
    if not path:
        return path_segments
    
    # Process each segment in the path
    for segment in path:
        # Skip zero-length segments
        if segment.length() < 1e-6:
            continue
        
        # Check if this segment starts where the previous one ended
        # If not, it's a new subpath (from a "move to" command)
        start_point = complex(segment.start.real, segment.start.imag)
        
        if last_end_point is not None:
            # Check for a significant gap (indicating a "move to")
            distance = abs(start_point - last_end_point)
            if distance > 1e-6:  # Threshold for considering it a break
                if current_segment:  # If we have points in the current segment
                    path_segments.append(current_segment)
                    current_segment = []  # Start a new segment
        
        # Calculate number of points based on segment length
        segment_length = segment.length()
        num_points = max(3, min(points_per_segment, int(segment_length / 5) + 3))
        
        # Sample points evenly along the segment
        for i in range(num_points):
            t = i / (num_points - 1)
            point = segment.point(t)
            current_segment.append((point.real, point.imag))
        
        # Remember the end point of this segment
        last_end_point = complex(segment.end.real, segment.end.imag)
    
    # Add the last segment if it has points
    if current_segment:
        path_segments.append(current_segment)
    
    return path_segments

def normalize_points(points, global_bounds=None):
    """
    Normalize points to 0-1 range while preserving aspect ratio and relative positions.
    If global_bounds is provided, use those bounds for normalization instead of 
    calculating new bounds for this specific set of points.
    """
    if not points:
        return []
    
    if global_bounds:
        min_x, max_x, min_y, max_y = global_bounds
    else:
        # Find bounds
        min_x = min(p[0] for p in points)
        max_x = max(p[0] for p in points)
        min_y = min(p[1] for p in points)
        max_y = max(p[1] for p in points)
    
    # Calculate width and height
    width = max_x - min_x
    height = max_y - min_y
    
    # Prevent division by zero
    if width < 1e-6 or height < 1e-6:
        return [(0.5, 0.5) for _ in points]
    
    # Use the larger dimension to normalize to preserve aspect ratio
    max_dim = max(width, height)
    
    # Center in the 0-1 range
    x_offset = (max_dim - width) / 2 / max_dim
    y_offset = (max_dim - height) / 2 / max_dim
    
    # Normalize all points preserving aspect ratio
    normalized = []
    for x, y in points:
        nx = (x - min_x) / max_dim + x_offset
        ny = (y - min_y) / max_dim + y_offset
        normalized.append((nx, ny))
    
    return normalized

def generate_debug_svg(path_segments, filename, width=500, height=500):
    """
    Generate a debug SVG showing the path segments.
    """
    if not filename or not path_segments:
        return
    
    # Collect all points
    all_points = []
    for segment in path_segments:
        all_points.extend(segment)
    
    if not all_points:
        print("No points to render in debug SVG")
        return
    
    # Find bounds
    min_x = min(p[0] for p in all_points)
    max_x = max(p[0] for p in all_points)
    min_y = min(p[1] for p in all_points)
    max_y = max(p[1] for p in all_points)
    
    # Calculate scale to fit SVG
    width_ratio = width / (max_x - min_x) if max_x > min_x else 1
    height_ratio = height / (max_y - min_y) if max_y > min_y else 1
    scale = min(width_ratio, height_ratio) * 0.9  # Leave some margin
    
    # Calculate offset to center
    offset_x = (width - (max_x - min_x) * scale) / 2 - min_x * scale
    offset_y = (height - (max_y - min_y) * scale) / 2 - min_y * scale
    
    # Generate SVG
    svg = f"""<?xml version="1.0" encoding="UTF-8"?>
<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">
  <rect width="{width}" height="{height}" fill="#eee" />
"""
    
    # Add each path segment with a different color
    colors = ["blue", "red", "green", "purple", "orange", "cyan", "magenta"]
    for i, segment in enumerate(path_segments):
        if not segment:
            continue
            
        color = colors[i % len(colors)]
        
        # Create polyline for this segment
        svg += f'  <polyline points="'
        for x, y in segment:
            px = x * scale + offset_x
            py = y * scale + offset_y
            svg += f"{px},{py} "
        svg += f'" fill="none" stroke="{color}" stroke-width="2" />\n'
        
        # Add a small marker at the start point
        if segment:
            x, y = segment[0]
            px = x * scale + offset_x
            py = y * scale + offset_y
            svg += f'  <circle cx="{px}" cy="{py}" r="3" fill="{color}" opacity="0.5" />\n'
    
    svg += "</svg>"
    
    # Write the file
    with open(filename, 'w') as f:
        f.write(svg)
    
    print(f"Debug SVG written to {filename} with {len(path_segments)} segments")

def generate_header(segments, name="svg", scale=255.0):
    """
    Generate a C header file with separate arrays for each path segment.
    """
    # Count points and segments
    segment_point_counts = [len(segment) for segment in segments]
    total_points = sum(segment_point_counts)
    num_segments = len(segments)
    
    # Generate header guard
    header_guard = f"{name.upper()}_POINTS_H"
    
    # Generate separate arrays for each path segment
    path_arrays = []
    for i, segment in enumerate(segments):
        segment_name = f"{name}_path_{i+1}"
        segment_array = f"// Path segment {i+1} with {len(segment)} points\n"
        segment_array += f"const float {segment_name}[] = {{\n"
        
        for j, (x, y) in enumerate(segment):
            if j % 4 == 0:
                segment_array += "    "
            segment_array += f"{x:.6f}f, {y:.6f}f, "
            if j % 4 == 3 or j == len(segment) - 1:
                segment_array += "\n"
        
        segment_array += "};\n"
        segment_array += f"#define {name.upper()}_PATH_{i+1}_POINT_COUNT {len(segment)}\n\n"
        path_arrays.append(segment_array)
    
    # Combine all path arrays
    all_path_arrays = "\n".join(path_arrays)
    
    # Generate counts and scale factor
    total_point_count = f"#define {name.upper()}_TOTAL_POINT_COUNT {total_points}\n"
    path_count = f"#define {name.upper()}_PATH_COUNT {num_segments}\n"
    scale_factor = f"#define {name.upper()}_SCALE {scale}f\n\n"
    
    # Create a path pointers array
    path_pointers = f"// Array of pointers to each path segment\n"
    path_pointers += f"const float* {name}_paths[] = {{\n"
    for i in range(num_segments):
        path_pointers += f"    {name}_path_{i+1},\n"
    path_pointers += "};\n\n"
    
    # Create a path lengths array
    path_lengths = f"// Array of point counts for each path segment\n"
    path_lengths += f"const uint16_t {name}_path_lengths[] = {{\n"
    for i in range(num_segments):
        path_lengths += f"    {name.upper()}_PATH_{i+1}_POINT_COUNT,\n"
    path_lengths += "};\n"
    
    # Combine everything
    header = f"""#ifndef {header_guard}
#define {header_guard}

#include <stdbool.h>
#include <stdint.h>

// Scale factor for all paths
{scale_factor}

// Total number of paths and points
{path_count}
{total_point_count}

// Individual path data - separate array for each path
{all_path_arrays}

{path_pointers}

{path_lengths}

#endif // {header_guard}
"""
    
    return header

def main():
    parser = argparse.ArgumentParser(description='Convert SVG path to points for oscilloscope display')
    parser.add_argument('input', help='Input SVG file')
    parser.add_argument('output', help='Output header file')
    parser.add_argument('--name', default='dvd_logo', help='Variable name prefix (default: dvd_logo)')
    parser.add_argument('--scale', type=float, default=255.0, help='Scale factor (default: 255.0)')
    parser.add_argument('--points-per-segment', type=int, default=50, 
                        help='Number of points per path segment (default: 50)')
    parser.add_argument('--debug-svg', help='Output a debug SVG file showing the path segments')
    args = parser.parse_args()
    
    print(f"Processing SVG file: {args.input}")
    
    # Extract paths from SVG
    parsed_paths = extract_paths_from_svg(args.input)
    print(f"Extracted {len(parsed_paths)} paths from the SVG")
    
    # Convert each path to discrete points
    path_segments = []
    for path_data in parsed_paths:
        segments = path_to_discrete_points(path_data['path'], args.points_per_segment)
        if segments:
            path_segments.extend(segments)
            total_points = sum(len(segment) for segment in segments)
            print(f"Path '{path_data['id']}': {len(segments)} segments, {total_points} total points")
    
    # Exit if no valid paths found
    if not path_segments:
        print("No valid path segments found")
        sys.exit(1)
    
    # Calculate global bounds from all path segments
    all_points = []
    for segment in path_segments:
        all_points.extend(segment)
        
    min_x = min(p[0] for p in all_points)
    max_x = max(p[0] for p in all_points)
    min_y = min(p[1] for p in all_points)
    max_y = max(p[1] for p in all_points)
    global_bounds = (min_x, max_x, min_y, max_y)
    
    # Normalize each segment using the same global bounds
    normalized_segments = [normalize_points(segment, global_bounds) for segment in path_segments]
    
    # Generate debug SVG if requested
    if args.debug_svg:
        generate_debug_svg(normalized_segments, args.debug_svg)
    
    # Generate header file
    header = generate_header(normalized_segments, args.name, args.scale)
    
    # Write header file
    with open(args.output, 'w') as f:
        f.write(header)
    
    # Calculate stats
    total_points = sum(len(segment) for segment in normalized_segments)
    print(f"Successfully converted {args.input} to {args.output}")
    print(f"Generated {total_points} points across {len(normalized_segments)} path segments")
    print(f"Scale factor: {args.scale}")

if __name__ == '__main__':
    main()