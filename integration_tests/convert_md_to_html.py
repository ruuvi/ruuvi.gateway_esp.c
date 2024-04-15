#!/usr/bin/env python3

import markdown
import sys


def convert_md_to_html(md_filepath, html_filepath):
    """
    Convert a Markdown file to an HTML file.

    Args:
    - md_filepath: Path to the source Markdown file.
    - html_filepath: Path to the destination HTML file.
    """
    # Read the Markdown file
    with open(md_filepath, 'r', encoding='utf-8') as md_file:
        md_content = md_file.read()

    # Convert Markdown to HTML
    # html_content = markdown.markdown(md_content)
    html_content = markdown.markdown(md_content, extensions=['extra', 'codehilite'])

    # Write the HTML content to the destination file
    with open(html_filepath, 'w', encoding='utf-8') as html_file:
        html_file.write(html_content)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_md_to_html.py <source_md_file> <destination_html_file>")
    else:
        md_file, html_file = sys.argv[1], sys.argv[2]
        convert_md_to_html(md_file, html_file)
        print(f"Converted {md_file} to HTML and saved as {html_file}.")
