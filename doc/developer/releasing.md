<!--
    SPDX-FileCopyrightText: 2026 n4n0
    SPDX-License-Identifier: CC0-1.0
-->

# Releasing

A release of Indeks is a tag with a release on GitHub. Publishing the release has GitHub
Actions build it in the two forms people install, and attach them:

| Form | Workflow | What is attached | How it updates |
| --- | --- | --- | --- |
| Flatpak | `.github/workflows/flatpak.yml` | `indeks.flatpak` | The workflow also publishes a Flatpak repository on GitHub Pages (`https://n4n0gh.github.io/Indeks`). Installed copies update from it with the rest of the system's Flatpaks. |
| AppImage | `.github/workflows/appimage.yml` | `Indeks-<version>-x86_64.AppImage` and its `.zsync` | The AppImage carries where its releases are. AppImageUpdate, Gear Lever, AppImageLauncher and the like read that, look at the newest release and fetch what changed through the `.zsync` file. |

## Making a release

1. **Version.** Set it in `project(indeks VERSION …)` in the top `CMakeLists.txt`. The
   application, the manual's cover and the name of the AppImage all take it from there.
2. **Release notes for software centres.** Add a `<release>` with the version, the date
   and a few words to `io.github.n4n0gh.indeks.metainfo.xml`.
3. **Manual.** Give the chapter *Changes* (`doc/manual/13-changes.md`) a heading for the
   version, above the one before it.
4. **Check.** `cmake --build build && ctest --test-dir build`, and a look at the manual.
5. **Tag and publish.**

   ```sh
   git tag v1.2.3
   git push origin master v1.2.3
   gh release create v1.2.3 --title "Indeks 1.2.3" --notes "What is new …"
   ```

   The two workflows start when the release is published, not when the tag is pushed. A
   release marked as a pre-release is built and offered as an update like any other.
6. **Look at the result.** The release should end up with three files: `indeks.flatpak`,
   the AppImage and its `.zsync`.

Both workflows can also be started by hand from the *Actions* page of the repository, to
see whether a build works. What they build is then kept with the run for download and not
attached to anything; the Flatpak one asks whether to publish to the repository.

## Set up once, in the repository's settings on GitHub

- **Pages:** *Settings → Pages → Source: GitHub Actions*. Without it the Flatpak workflow
  builds and attaches the bundle but cannot publish the repository.
- **The signing key:** the repository secret `FLATPAK_GPG_PRIVATE_KEY`; see below.

### The signing key

Everything the Flatpak workflow publishes is signed with a GPG key made for this purpose:
`Indeks Flatpak repository`, Ed25519, without an end date, fingerprint
`4A74 75FF 2642 EEA6 ADAF  6000 0E21 F121 640F 1BDC`.

- Its secret half is the repository secret `FLATPAK_GPG_PRIVATE_KEY`: ASCII-armoured,
  without a passphrase.
- Its public half is in `indeks.flatpakref`, `indeks.flatpakrepo` and the bundle, and from
  there in every installed copy, which checks what it downloads against it.
- GitHub does not give secrets back. **Whoever maintains the releases has to keep a copy of
  the secret key.** If it is lost, a new key has to be made, and everyone who installed
  Indeks has to remove the remote and install again, because their copies only trust the
  old one.
- To put a key in place: `gh secret set FLATPAK_GPG_PRIVATE_KEY --repo n4n0GH/Indeks < secret-key.asc`.

Without the secret the workflow still runs, and publishes an unsigned repository. Copies
that already check signatures then refuse the update, so that is only an option for a
repository nobody has installed from yet.

## What must stay as it is

- **The names of the AppImage and its `.zsync`.** Installed AppImages look for
  `Indeks-*-x86_64.AppImage.zsync` in the newest release of `n4n0GH/Indeks`
  (`gh-releases-zsync|n4n0GH|Indeks|latest-all|…`, set in the build script). A release
  without that file, or with another name for it, is one they do not see. Should the
  repository ever move, AppImages that are already out there keep looking in the old place.
- **The address of the Flatpak repository** (`SITE_URL` in the workflow). Installed copies
  remember it.
- **The name and the mark of the index:** `index.sqlite` and `Database::ApplicationId`.
  Every database that exists is found by the one and recognised by the other.
- **The runtime.** `runtime-version` in `flatpak/io.github.n4n0gh.indeks.yml`,
  `base-version` below it and the tag of the container image in the workflow belong
  together.

## The Flatpak

`flatpak/io.github.n4n0gh.indeks.yml` builds Indeks on the KDE runtime. Qt PDF comes from
Flathub's QtWebEngine base app, of which the web engine itself is removed again. Tesseract is
built along, with English and German.

Not there inside the sandbox, and so not available in the Flatpak: LibreOffice (Office
documents are shown as their text, and the old Office formats are found by name only) and
*Send to Indeks* in Dolphin. Bubblewrap is not there either, and scripts run all the same:
see *Scripts in the Flatpak* in [architecture.md](architecture.md). The manual says so under
*Getting Started → Installing Indeks*.

```sh
flatpak-builder --user --install --install-deps-from=flathub --force-clean build-flatpak flatpak/io.github.n4n0gh.indeks.yml
flatpak run io.github.n4n0gh.indeks
```

`flatpak/make-site.sh` turns the repository that flatpak-builder exports into the web site
that is published: the repository, an `indeks.flatpakref` to install from, and a bundle
that updates from the same place.

## The AppImage

`packaging/appimage/build-appimage.sh` builds Indeks in `build-appimage/` and bundles it
with linuxdeploy into `dist/`. It is the same script here and in the workflow, which runs
it in an openSUSE Tumbleweed container.

```sh
packaging/appimage/build-appimage.sh
dist/Indeks-*-x86_64.AppImage
```

- Qt, Qt PDF, the KDE libraries, their QML modules and the plugins Indeks needs are in the
  AppImage. Tesseract, LibreOffice, Python and bubblewrap are not: Indeks uses the system's
  where they are installed, as it does when installed normally.
- The C library of the build system is newer than what many systems have. The AppImage
  carries it and starts through it on a system with an older one; on any other it uses
  the system's. The helper program that reads files goes the same way.
- The container image and the package repository of Tumbleweed are not always of the same
  day. The workflow therefore begins with `zypper dup --allow-downgrade`, which puts the
  image at what the repository has. Without it, a day on which the image is ahead ends
  with packages that cannot be installed, before anything is built.
- A package the workflow is missing shows as a build that fails, or as an AppImage that
  starts without something: a QML module, an icon engine, a way to reach servers. What
  the script copies by hand is listed in it with the reason; packages for it go into the
  workflow's list.
