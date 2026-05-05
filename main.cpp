#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <windows.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

enum Material{
    AIR,   // 0 - empty
    SAND,  // Falls and clumps
    METAL, // Holds in the same place
    WATER, // Flattens in areas, flows off left and right
    DIRT   // Falls but not left / right
};

const int width = 600; // Window width
const int height = 400; // Window height
const int popupX = 20;
const int popupY = 20;
const float shadeMin = 0.95f;
const float shadeRange = 0.1f;

// Settings:
bool paused = false; // Pauses stepping
int ticksPerSecond = 500; // Tick speed (Steps / second)
Material material = SAND; // Material

uint8_t grid[width * height] = {};
uint8_t next[width * height] = {};

// Create shades grid
float noise[width * height];

std::string appdata = std::getenv("LOCALAPPDATA");
std::string folder = appdata + "\\2D-WorldBox";
std::string gridSaveDir = folder + "\\grid.bin";

static float saveErrorPopupTimer = 0.0f;
static float saveSuccessPopupTimer = 0.0f;
static float loadErrorPopupTimer = 0.0f;
static float loadSuccessPopupTimer = 0.0f;

void step(){
    memset(next, AIR, sizeof(next));
    for (int y = height - 1; y >= 0; y--){
        for (int x = 0; x < width; x++){
            if (grid[y * width + x] != AIR){
                switch (grid[y * width + x]){
                    case SAND:{
                        if (y + 1 < height && grid[(y + 1) * width + x] == AIR){
                            next[(y + 1) * width + x] = SAND;
                        } else {
                            bool leftFirst = rand() % 2;
                            bool slid = false;

                            int first  = leftFirst ? x - 1 : x + 1;
                            int second = leftFirst ? x + 1 : x - 1;

                            if (y + 1 < height && first >= 0 && first < width && grid[(y + 1) * width + first] == AIR){
                                next[(y + 1) * width + first] = SAND;
                                slid = true;
                            } else if (y + 1 < height && second >= 0 && second < width && grid[(y + 1) * width + second] == AIR){
                                next[(y + 1) * width + second] = SAND;
                                slid = true;
                            }

                            if (!slid){
                                next[y * width + x] = SAND;
                            }
                        }
                        break;
                    }
                    case METAL:{
                        next[y * width + x] = METAL;
                        break;
                    }
                    case DIRT:{
                        if (y + 1 < height && grid[(y + 1) * width + x] == AIR){
                            next[(y + 1) * width + x] = DIRT;
                        } else {
                            bool leftFirst = rand() % 2;
                            bool slid = false;
                            int first  = leftFirst ? x - 1 : x + 1;
                            int second = leftFirst ? x + 1 : x - 1;
                            if (y + 1 < height && first >= 0 && first < width && grid[(y + 1) * width + first] == AIR){
                                next[(y + 1) * width + first] = DIRT;
                                slid = true;
                            } else if (y + 1 < height && second >= 0 && second < width && grid[(y + 1) * width + second] == AIR){
                                next[(y + 1) * width + second] = DIRT;
                                slid = true;
                            }
                            if (!slid){
                                next[y * width + x] = DIRT;
                            }
                        }
                        break;
                    }
                    case WATER:{
                        if (y + 1 < height && grid[(y + 1) * width + x] == AIR && next[(y + 1) * width + x] == AIR){
                            next[(y + 1) * width + x] = WATER;
                        } else {
                            bool leftFirst = rand() % 2;
                            bool slid = false;

                            int first  = leftFirst ? x - 1 : x + 1;
                            int second = leftFirst ? x + 1 : x - 1;

                            if (y + 1 < height && first >= 0 && first < width && grid[(y + 1) * width + first] == AIR && next[(y + 1) * width + first] == AIR){
                                next[(y + 1) * width + first] = WATER;
                                slid = true;
                            } else if (y + 1 < height && second >= 0 && second < width && grid[(y + 1) * width + second] == AIR && next[(y + 1) * width + second] == AIR){
                                next[(y + 1) * width + second] = WATER;
                                slid = true;
                            } else {
                                int dirs[2] = { leftFirst ? -1 : 1, leftFirst ? 1 : -1 };
                                for (int d : dirs) {
                                    int spread = 0;
                                    for (int dist = 1; dist <= ticksPerSecond * 0.0166f; dist++) {
                                        int nx = x + d * dist;
                                        if (nx < 0 || nx >= width) break;
                                        if (grid[y * width + nx] == AIR && next[y * width + nx] == AIR) {
                                            spread = nx;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (spread != 0) {
                                        next[y * width + spread] = WATER;
                                        slid = true;
                                        break;
                                    }
                                }
                            }
                            if (!slid){
                                next[y * width + x] = WATER;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    memcpy(grid, next, sizeof(grid));
}

int saveGrid(){
    std::ofstream out(gridSaveDir, std::ios::binary);
    if (!out) return 0;
    out.write(reinterpret_cast<char*>(grid), width * height);
    return 1;
}

int loadGrid(){
    std::ifstream in(gridSaveDir, std::ios::binary);
    if (!in) return 0;
    in.read(reinterpret_cast<char*>(grid), width * height);
    return 1;
}

int main() {
    CreateDirectoryA(folder.c_str(), NULL);
    for (int i = 0; i < width * height; i++)
        noise[i] = shadeMin + (rand() / (float)RAND_MAX) * shadeRange;

    int loaded = loadGrid();
    if (!loaded){
        std::cout << "Failed Loading saved grid\n"; 
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Sand and stuff", width, height, 0); // Create window
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr); // Create renderer

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_Event e;

    Uint64 lastTime = SDL_GetTicks();
    float accumulator = 0.0f;

    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)){ // Check if closed
            ImGui_ImplSDL3_ProcessEvent(&e); // Give imgui events
            if (e.type == SDL_EVENT_QUIT){
                running = false;
            }
        }
        if (!ImGui::GetIO().WantCaptureMouse){
            float mx, my;

            SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
            if (buttons & SDL_BUTTON_LMASK){
                int gx = int(mx);
                int gy = int(my);
                if (gx >= 0 && gx < width && gy >= 0 && gy < height){
                    grid[gy * width + gx] = material;
                } 
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");
        ImGui::Checkbox("Pause", &paused);
        ImGui::Spacing();
        if (ImGui::Button("Reset")){
            memset(grid, 0, width * height);
        }
        ImGui::Spacing();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Ticks Per Second", &ticksPerSecond, 10);
        ImGui::Separator();
        ImGui::Text("Material");
        ImGui::Spacing();
        ImGui::RadioButton("Sand", (int*)&material, SAND);
        ImGui::RadioButton("Dirt", (int*)&material, DIRT);
        ImGui::RadioButton("Water", (int*)&material, WATER);
        ImGui::RadioButton("Metal", (int*)&material, METAL);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Save");
        if (ImGui::Button("Save")){
            int saved = saveGrid();
            if (saved){
                saveSuccessPopupTimer += 2.0f;
            }else{
                saveErrorPopupTimer += 2.0f;
            }
        }
        if (ImGui::Button("Load")){
            int loaded = loadGrid();
            if (loaded){
                loadSuccessPopupTimer += 2.0f;
            }else{
                loadErrorPopupTimer += 2.0f;
            }
        }
        ImGui::End();

        if (saveErrorPopupTimer > 0.0f) {
            saveErrorPopupTimer -= ImGui::GetIO().DeltaTime;
            ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
            ImGui::Begin("##toast1", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Error saving");
            ImGui::End();
        }else{
            if (saveSuccessPopupTimer > 0.0f) {
                saveSuccessPopupTimer -= ImGui::GetIO().DeltaTime;
                ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
                ImGui::Begin("##toast2", nullptr,
                    ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("Saved");
                ImGui::End();
            }
        }

        if (loadErrorPopupTimer > 0.0f) {
            loadErrorPopupTimer -= ImGui::GetIO().DeltaTime;
            ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
            ImGui::Begin("##toast3", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Error loading");
            ImGui::End();
        }else{
            if (loadSuccessPopupTimer > 0.0f) {
                loadSuccessPopupTimer -= ImGui::GetIO().DeltaTime;
                ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
                ImGui::Begin("##toast4", nullptr,
                    ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("Loaded");
                ImGui::End();
            }
        }

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set color to black
        SDL_RenderClear(renderer); // Clear renderer

        Uint64 now = SDL_GetTicks();
        float elapsed = (now - lastTime) / 1000.0f;
        lastTime = now;

        if (!paused) {
            float stepInterval = 1.0f / ticksPerSecond;
            accumulator += elapsed;
            while (accumulator >= stepInterval) {
                step();
                accumulator -= stepInterval;
            }
        }

        for (int i = 0; i < width * height; i++) { // Iterate through grid
            int x = i % width; // Resolve x
            int y = i / width; // Resolve y

            if (grid[i] != AIR){
                switch (grid[i]){
                    case SAND:{
                        SDL_SetRenderDrawColor(renderer, 194 * noise[i], 178 * noise[i], 128 * noise[i], 255); // Set color to sandy
                        break;
                    }
                    case METAL:{
                        SDL_SetRenderDrawColor(renderer, 83, 86 * noise[i], 84 * noise[i], 255); // Set to metallic silver
                        break;
                    }
                    case DIRT:{
                        bool grassOnTop = (y - 1 < 0 || grid[(y - 1) * width + x] == AIR);
                        if (grassOnTop){
                            SDL_SetRenderDrawColor(renderer, 86, 130 * noise[i], 3, 255);   // Set to grass
                        } else {
                            SDL_SetRenderDrawColor(renderer, 107 * noise[i], 84 * noise[i], 40, 255);  // Set to dirt
                        }
                        break;
                    }
                    case WATER:{
                        SDL_SetRenderDrawColor(renderer, 28, 163, 236 * noise[i], 255); // Set color to blue
                        break;
                    }
                    default:
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Debug color (unreachable)
                        break;
                }
            SDL_RenderPoint(renderer, x, y); // Render at x, y
            }
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer); // Place imgui window
        SDL_RenderPresent(renderer); // Render the present renderer obj
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_Quit(); // Quit
    return 0; // Return
}