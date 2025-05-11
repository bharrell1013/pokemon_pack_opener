# Packaging Instructions for Pokemon Pack Simulator

This document explains how to package the Pokemon Pack Simulator for distribution on your website.

## Prerequisites

- Windows 10 or newer
- PowerShell 5.0 or newer

## Steps to Create the Package

1. **Run the Packaging Script**

   Open PowerShell in the root directory of your project and run:
   
   ```powershell
   .\package.ps1
   ```
   
   This will create a directory named `PokemonPackSimulator` and a zip file named `PokemonPackSimulator.zip`.

2. **Verify the Package**

   Extract the zip file and test that the application runs correctly:
   
   - Double-click on `Run_Pokemon_Pack_Simulator.bat`
   - Verify that all textures load correctly
   - Test the main functionality

3. **Create a Screenshot**

   Take a screenshot of your application running to use on your website. Save it as `screenshot.jpg`.

4. **Upload to Your Website**

   1. Upload both the `PokemonPackSimulator.zip` file and `screenshot.jpg` to your website
   2. Upload the `website-download-page.html` file (rename it as needed, e.g., to `download.html`)
   3. Link to the download page from your main website navigation

## Troubleshooting Package Creation

### Missing Files

If certain files are missing when you run the script:

1. Check the paths in the script
2. Ensure all required files exist in the mentioned directories
3. Modify the script to skip missing files or display a warning

### Large Package Size

If the package is too large:

1. Consider removing unnecessary models or textures
2. Use image compression for textures
3. Create a "lite" version with fewer assets

## Updating the Package

When you update your application:

1. Rebuild the application
2. Update the version number in the `website-download-page.html` file
3. Run the packaging script again
4. Upload the new zip file to your website

## Additional Distribution Options

### Creating an Installer

For a more professional distribution, consider creating an installer:

1. Use [Inno Setup](https://jrsoftware.org/isinfo.php) (free) or [NSIS](https://nsis.sourceforge.io/Main_Page) (free)
2. Create an installer script that:
   - Installs the application to a user-chosen directory
   - Creates desktop and start menu shortcuts
   - Registers file associations if needed
   - Adds uninstall functionality

### GitHub Releases

If your project is on GitHub, consider using GitHub Releases:

1. Create a new release on GitHub
2. Upload the zip file as an asset
3. Provide release notes and documentation
4. Link to the GitHub release from your website

## Distribution Policies

Remember to include:

1. Clear licensing information
2. Any third-party attributions
3. Disclaimer regarding Pokemon trademarks
4. System requirements
5. Contact information for support 