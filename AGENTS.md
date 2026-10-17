# Repository layout

- This directory is the standalone Git repository; keep sources, documentation,
  build configuration, tools, licenses and original application assets here.
- Keep all generated files outside the repository, in `../out`.
- Configure Release with `../out/build/x64-Release`, Debug with
  `../out/build/x64-Debug`; executables belong in `../out/bin/<configuration>`.
- Do not create CMake build trees, binaries, simulation exports, blog packages,
  screenshot capture intermediates or Python bytecode inside the repository.
- Documentation images in `docs/images` and icon assets in `resources/icons`
  are versioned publication/application resources, not build outputs.
