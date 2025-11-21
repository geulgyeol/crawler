# Docker Support

This repository includes Docker support for building and running the crawler applications across multiple architectures (ARM64 and AMD64).

## Building Locally

To build the Docker image locally:

```bash
# Build for your current platform
docker build -t crawler .

# Build for specific platform
docker build --platform linux/amd64 -t crawler:amd64 .
docker build --platform linux/arm64 -t crawler:arm64 .
```

## Running the Applications

The Docker image contains three applications:
- LinkFinder
- ProfileFinder
- HTMLCrawler

To run a specific application:

```bash
# Run LinkFinder (default)
docker run --rm crawler

# Run ProfileFinder
docker run --rm crawler /app/ProfileFinder

# Run HTMLCrawler
docker run --rm crawler /app/HTMLCrawler
```

## Multi-Architecture Builds

The repository is configured with GitHub Actions to automatically build and push multi-architecture images to GitHub Container Registry (ghcr.io).

### Available Images

Images are published to: `ghcr.io/geulgyeol/crawler` (or `ghcr.io/<owner>/<repo>` based on the repository)

The image name is automatically derived from the repository using `${{ github.repository }}`.

Tags:
- `latest` - Latest build from the default branch
- `main` or `master` - Latest build from the respective branch
- `v*` - Semantic version tags (e.g., `v1.0.0`, `v1.0`, `v1`)
- `pr-*` - Pull request builds

### Pulling Images

```bash
# Pull the latest image (supports both ARM64 and AMD64)
docker pull ghcr.io/geulgyeol/crawler:latest

# Pull a specific version
docker pull ghcr.io/geulgyeol/crawler:v1.0.0

# Run from registry
docker run --rm ghcr.io/geulgyeol/crawler:latest
```

## GitHub Actions Workflow

The workflow is triggered on:
- Push to `main` or `master` branches
- Push of version tags (e.g., `v1.0.0`)
- Pull requests
- Manual workflow dispatch

The workflow:
1. Checks out the code
2. Sets up QEMU for multi-architecture builds
3. Sets up Docker Buildx
4. Logs in to GitHub Container Registry using:
   - Username: `${{ github.actor }}` (the user who triggered the workflow)
   - Password: `${{ secrets.GH_TOKEN }}` secret
5. Automatically generates image name from repository: `${{ github.repository }}`
6. Builds images for both ARM64 and AMD64
7. Pushes images to ghcr.io (except for PRs)
8. Uses GitHub Actions cache for faster subsequent builds

## Architecture

The Dockerfile uses a multi-stage build:

1. **Builder Stage**: 
   - Based on Debian Bookworm
   - Installs build dependencies
   - Sets up vcpkg for C++ package management
   - Installs google-cloud-cpp and curl
   - Builds all three applications using CMake

2. **Runtime Stage**:
   - Minimal Debian Bookworm slim image
   - Contains only runtime dependencies
   - Includes the compiled binaries
   - Significantly smaller than the builder image

## Cross-Platform Compatibility

The code includes a compatibility layer (`Library/platform_compat.h`) to handle platform-specific differences between Windows and Linux, particularly for console encoding functions.

## Requirements

To build and run the Docker images, you need:
- Docker 20.10 or later
- For multi-architecture builds: Docker Buildx and QEMU

## Development

To make changes and test locally:

1. Modify the source code
2. Build the Docker image:
   ```bash
   docker build -t crawler:dev .
   ```
3. Test the application:
   ```bash
   docker run --rm crawler:dev
   ```

## Environment Variables

The applications use configuration defined in `Library/config.cpp`. To customize:
- Modify the `Config` class constructor
- Or pass environment variables (requires code changes to support)

## Troubleshooting

### Build fails with vcpkg errors
- Ensure you have enough disk space (vcpkg downloads can be large)
- Check your internet connection (vcpkg needs to download dependencies)

### Runtime errors about missing libraries
- The runtime stage should include all necessary libraries
- If you added new dependencies, update both the builder and runtime stages

### Multi-architecture build is slow
- First builds are always slower
- GitHub Actions cache significantly speeds up subsequent builds
- Local multi-architecture builds require emulation and will be slower
