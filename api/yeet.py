import requests
from urllib.parse import unquote
from pathlib import Path
from tqdm import tqdm
import random
from typing import List, Tuple


def get_filename_from_url(url):
    response = requests.head(url)
    content_disposition = response.headers.get("Content-Disposition")

    if content_disposition and "filename=" in content_disposition:
        return unquote(content_disposition.split("filename=")[1].strip('"'))
    else:
        return url.split("/")[-1]


def download_file(url, destination=Path(".")):
    file_name = get_filename_from_url(url)
    destination /= file_name
    print(f"downloading {destination}")
    response = requests.get(url, stream=True)

    total_size = int(response.headers.get("content-length", 0))
    block_size = 1024  # 1 Kibibyte

    with tqdm(total=total_size, unit="B", unit_scale=True) as progress_bar:
        with open(destination, "wb") as file:
            for data in response.iter_content(block_size):
                progress_bar.update(len(data))
                file.write(data)
    return file_name


def split_and_download(
    links_and_sizes: List[Tuple[str, float]], percentage_split: float, folder: Path
):
    random.shuffle(links_and_sizes)

    total_size = sum(size for _, size in links_and_sizes)
    target_size = total_size * percentage_split

    downloaded_size = 0
    used_links = set()
    files = []
    buffer_size = 200

    while downloaded_size < (target_size - buffer_size) and len(used_links) < len(
        links_and_sizes
    ):
        # If there are still files left and the target size is not reached, shuffle and try again
        random.shuffle(links_and_sizes)

        downloaded = False
        for link, size in links_and_sizes:
            if downloaded_size + size > target_size or link in used_links:
                continue

            files.append(download_file(link, folder))
            downloaded_size += size
            used_links.add(link)
            downloaded = True

        if not downloaded:
            break

    # If no files can be downloaded within the buffer range, download the smallest files
    if downloaded_size < (target_size - buffer_size):
        remaining_links = [
            link for link, size in links_and_sizes if link not in used_links
        ]
        remaining_links.sort(key=lambda x: x[1])  # Sort by file size in ascending order

        for link, size in remaining_links:
            if downloaded_size + size > target_size + buffer_size:
                break

            files.append(download_file(link, folder))
            downloaded_size += size
            used_links.add(link)

    return files
