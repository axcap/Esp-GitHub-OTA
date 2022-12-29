# Esp-GitHub-OTA

Inspired by [esp_ghota](https://github.com/Fishwaldo/esp_ghota) by **Fishwaldo**

Brings same functionality to Arduino platform


# How to get started
1. Create github repository and clone it locally (**root_dir**)
2. Open one of the examples provided with this library and save it to the newly cloned directory
3. Replace VERSION (line 7) and RELEASE_URL (line 11) with your values
4. Additionally clone following files from this repo as Arduino does not do it for us
    * platformio.ini
    * .gitignore
    * .github (this folder contains GitHub Actions that will create releases of your OTA firmware)
5. Commit and push all staged files (run 'git add -am "First commit" && git push' from terminal from **root_dir** (see step 1))
6. Github actions will trigger automatically to check for any errors in the code, but no release will be created yet
7. When you are happy with your code create git tag by runnig 'git tag v0.0.1' for version 0.0.1
8. Push your tag with 'git push origin v0.0.1', this will trigger Release creation
9. Update DELAY_MS (line 13) to 100 and VERSION (line 7) to 0.0.2
10. Repeat steps 5-8 to create new release and wait for your microcontroller to pick it up
11. Led should toggle much faster 🚨

This library is under development and may not work every time. <br />
Feel free to open Issues or create Pull requests :) 
