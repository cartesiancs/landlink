export const GITHUB_REPO = { owner: "hjunhuh", name: "landlink" } as const;

export const FIRMWARE_TAG_PREFIX = "fw-v";

export const FIRMWARE_RELEASES_URL = `https://api.github.com/repos/${GITHUB_REPO.owner}/${GITHUB_REPO.name}/releases?per_page=20`;
