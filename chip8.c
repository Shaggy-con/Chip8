#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>
#include <time.h>

typedef struct{
    SDL_Window *window;
    SDL_Renderer *renderer; 
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
}sdl_t;

typedef struct{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_factor;
    uint32_t inst_per_sec;
    uint32_t square_wave_freq;
    int16_t volume;
    uint32_t audio_sample_rate;
}config_t;

typedef enum{
    QUIT,
    RUNNING,
    PAUSED
}emulator_state_t;

typedef struct {
    uint16_t opcode;
    uint16_t NNN;   
    uint8_t NN;     
    uint8_t N;     
    uint8_t X;     
    uint8_t Y;      
} instruction_t;

typedef struct{
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64*32];
    uint16_t stack[12];
    uint16_t *stack_ptr;
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[16];
    char *rom_name;
    instruction_t inst;
    bool draw;
}chip8_t;


void audio_callback(void *userdata,uint8_t *stream,int len){
    int16_t *audio_data = (int16_t *)stream;
    config_t *config = (config_t *)userdata ;
    static uint32_t running_sample_index = 0;
    const int32_t square_wave_period = config->audio_sample_rate / config->square_wave_freq;
    const int32_t half_square_wave_period = square_wave_period/2;

    for (int i = 0; i < len / 2; i++){
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ? 
                        config->volume : 
                        -config->volume;
    }
}

bool init_sdl(sdl_t *sdl,const config_t *config){
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_AUDIO|SDL_INIT_VIDEO)!=0){
        SDL_Log("Couldnt initialize %s\n",SDL_GetError());
        return false;
    }
    sdl->window = SDL_CreateWindow("Chip 8 emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config->window_width*config->scale_factor,
        config->window_height*config->scale_factor,
        0);
    if(!sdl->window){
        SDL_Log("Couldnt Create Window %s\n",SDL_GetError());
        return false;
    }
    sdl->renderer = SDL_CreateRenderer(sdl->window,
                       -1, 
                       SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("Couldnt Render Window %s\n",SDL_GetError());
        return false;
    }

    sdl->want = (SDL_AudioSpec){
        .freq = 48000,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .samples = 4096,
        .callback = audio_callback,
        .userdata = config,
    };
    sdl->dev = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);
    if(!sdl->renderer){
        SDL_Log("Couldnt get Audio Device \n");
        return false;
    }
    return true;
}

bool init_chip8(chip8_t *chip8,char *rom_name){
    const uint32_t entry_point = 0x200;
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0
        0x20, 0x60, 0x20, 0x20, 0x70,		// 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3
        0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6
        0xF0, 0x10, 0x20, 0x40, 0x40,		// 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
        0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
        0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
        0xF0, 0x80, 0xF0, 0x80, 0x80		// F
    };
    memset(chip8, 0, sizeof(chip8_t));
    memcpy(&chip8->ram[0],font,sizeof(font));
    FILE *rom = fopen(rom_name,"rb");
    if(!rom){
        SDL_Log("Invalid Rom File %s\n",rom_name);
        return false;
    }

    fseek(rom,0,SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof(chip8->ram)-entry_point;
    rewind(rom);
    if(rom_size>max_size){
        SDL_Log("Rom File %s is too Large\n",rom_name);
        return false;
    }
    size_t x = fread(&chip8->ram[entry_point],rom_size,1,rom);
    if(x!=1){
        SDL_Log("Not able to load file %s %zu\n",rom_name,x);
        return false;
    }
    

    fclose(rom);


    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];
    return true;
}

void final_sdl(sdl_t *sdl){
    SDL_DestroyRenderer(sdl->renderer);
    SDL_DestroyWindow(sdl->window);
    SDL_CloseAudioDevice(sdl->dev);
    SDL_Quit();
}

bool set_config(config_t *config,const int argc,const char **argv){
    config->window_height = 32;
    config->window_width = 64;
    config->fg_color = 0xFFFFFFFF;
    config->bg_color=0x00000000;
    config->scale_factor = 20;
    config->inst_per_sec = 700;
    config->square_wave_freq = 440;
    config->volume = 3000;
    config->audio_sample_rate = 48000;
}

void clear_screen(const config_t config,const sdl_t sdl){
    uint8_t r=(uint8_t)(config.bg_color>>24)&0xFF;
    uint8_t g=(uint8_t)(config.bg_color>>16)&0xFF;
    uint8_t b=(uint8_t)(config.bg_color>>8)&0xFF;
    uint8_t a=(uint8_t)(config.bg_color>>0)&0xFF;

    SDL_SetRenderDrawColor(sdl.renderer,r,g,b,a);
    SDL_RenderClear(sdl.renderer);
}
void update_screen(const sdl_t sdl, const config_t config, chip8_t *chip8) {

    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >>  8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >>  0) & 0xFF;

    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >>  8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >>  0) & 0xFF;

    for (uint32_t i = 0; i < sizeof(chip8->display); i++) {
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8->display[i]) {
            SDL_SetRenderDrawColor(sdl.renderer,fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        } else {
            SDL_SetRenderDrawColor(sdl.renderer,bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }
    SDL_RenderPresent(sdl.renderer);
}

void handel_input(chip8_t *chip8){
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        switch (event.type)
        {
        case SDL_QUIT:
            chip8->state = QUIT;
            return;
        
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                chip8->state = QUIT;
                break;
            case SDLK_SPACE:
                if(chip8->state==RUNNING){
                    chip8->state=PAUSED;
                    printf("Paused\n");
                }
                else{
                    chip8->state=RUNNING;
                    printf("Resumed\n");
                }
                break;
            case SDLK_EQUALS:
                init_chip8(chip8,chip8->rom_name);
                printf("Game Reset\n");

            case SDLK_1:
                chip8->keypad[0x1] = true;
                break;
            case SDLK_2:
                chip8->keypad[0x2] = true;
                break;
            case SDLK_3:
                chip8->keypad[0x3] = true;
                break;
            case SDLK_4:
                chip8->keypad[0xC] = true;
                break;


            case SDLK_q:
                chip8->keypad[0x4] = true;
                break;
            case SDLK_w:
                chip8->keypad[0x5] = true;
                break;
            case SDLK_e:
                chip8->keypad[0x6] = true;
                break;
            case SDLK_r:
                chip8->keypad[0xD] = true;
                break;


            case SDLK_a:
                chip8->keypad[0x7] = true;
                break;
            case SDLK_s:
                chip8->keypad[0x8] = true;
                break;
            case SDLK_d:
                chip8->keypad[0x9] = true;
                break;
            case SDLK_f:
                chip8->keypad[0xE] = true;
                break;


            case SDLK_z:
                chip8->keypad[0xA] = true;
                break;
            case SDLK_x:
                chip8->keypad[0x0] = true;
                break;
            case SDLK_c:
                chip8->keypad[0xB] = true;
                break;
            case SDLK_v:
                chip8->keypad[0xF] = true;
                break;
            default:
                break;
            }
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {

            case SDLK_1:
                chip8->keypad[0x1] = false;
                break;
            case SDLK_2:
                chip8->keypad[0x2] = false;
                break;
            case SDLK_3:
                chip8->keypad[0x3] = false;
                break;
            case SDLK_4:
                chip8->keypad[0xC] = false;
                break;


            case SDLK_q:
                chip8->keypad[0x4] = false;
                break;
            case SDLK_w:
                chip8->keypad[0x5] = false;
                break;
            case SDLK_e:
                chip8->keypad[0x6] = false;
                break;
            case SDLK_r:
                chip8->keypad[0xD] = false;
                break;


            case SDLK_a:
                chip8->keypad[0x7] = false;
                break;
            case SDLK_s:
                chip8->keypad[0x8] = false;
                break;
            case SDLK_d:
                chip8->keypad[0x9] = false;
                break;
            case SDLK_f:
                chip8->keypad[0xE] = false;
                break;


            case SDLK_z:
                chip8->keypad[0xA] = false;
                break;
            case SDLK_x:
                chip8->keypad[0x0] = false;
                break;
            case SDLK_c:
                chip8->keypad[0xB] = false;
                break;
            case SDLK_v:
                chip8->keypad[0xF] = false;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

void emulate_instruction(chip8_t *chip8, const config_t config){
    bool carry;
    chip8->inst.opcode = (chip8->ram[chip8->PC]<<8)|chip8->ram[chip8->PC+1];
    chip8->PC+=2;
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

    //printf("%X %X\n",chip8->inst.opcode,chip8->I);
    switch ((chip8->inst.opcode>>12)&0x0F)
    {
    case 0x00:
        if(chip8->inst.NN == 0xE0){
            memset(&chip8->display[0],false,sizeof(chip8->display));
        }
        else if(chip8->inst.NN == 0xEE){
            chip8->PC = *--chip8->stack_ptr;
        }
        break;
    case 0x01:
        chip8->PC = chip8->inst.NNN;
        break;
    case 0x02:
        *chip8->stack_ptr++ = chip8->PC;
        chip8->PC = chip8->inst.NNN;
        break;
    case 0x03:
        if(chip8->V[chip8->inst.X]==chip8->inst.NN){
            chip8->PC += 2;
        }
        break;
    case 0x04:
        if(chip8->V[chip8->inst.X]!=chip8->inst.NN){
            chip8->PC += 2;
        }
        break;
    case 0x05:
        if (chip8->inst.N != 0) break;
        if(chip8->V[chip8->inst.X]==chip8->V[chip8->inst.Y]){
            chip8->PC += 2;
        }
        break;
    case 0x06:
        chip8->V[chip8->inst.X] = chip8->inst.NN;
        break;
    case 0x07:
        chip8->V[chip8->inst.X] += chip8->inst.NN;
        break;
    case 0x08:
        switch (chip8->inst.N)
        {
        case 0x00:
            chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
            break;
        case 0x01:
            chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
            chip8->V[0xF] = 0;
            break;
        case 0x02:
            chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
            chip8->V[0xF] = 0;
            break;
        case 0x03:
            chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
            chip8->V[0xF] = 0;
            break;
        case 0x04:
            carry = ((uint16_t)chip8->V[chip8->inst.X]+chip8->V[chip8->inst.Y] > 255);
            chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
            chip8->V[0xF] = carry;
            break;
        case 0x05:
            carry=(chip8->V[chip8->inst.Y]<=chip8->V[chip8->inst.X]);
            chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
            chip8->V[0xF] = carry;
            break;
        case 0x06:
            carry = chip8->V[chip8->inst.Y]&1;
            chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1;
            chip8->V[0xF] = carry;
            break;
        case 0x07:    
            carry = (chip8->V[chip8->inst.X]<=chip8->V[chip8->inst.Y]);
            chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
            chip8->V[0xF] = carry;
            break;
        case 0x0E:
            carry =  (chip8->V[chip8->inst.Y]&0x80)>>7;
            chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1;
            chip8->V[0xF] = carry;
            break;
        default:
            break;
        }
        
        break;
    case 0x09:
        if(chip8->V[chip8->inst.X]!=chip8->V[chip8->inst.Y]){
            chip8->PC += 2;
        }
        break;
    case 0x0A:
        chip8->I = chip8->inst.NNN;
        break;
    case 0x0B:
        chip8->PC = chip8->V[0]+chip8->inst.NNN;
        break; 
    case 0x0C:
        chip8->V[chip8->inst.X] = (rand()%256) &chip8->inst.NN;
        break; 
    case 0x0D: {
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_coord;
            chip8->V[0xF] = 0;
            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X;
                for (int8_t j = 7; j >= 0; j--) {
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord]; 
                    const bool sprite_bit = (sprite_data & (1 << j));
                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;  
                    }
                    *pixel ^= sprite_bit;
                    if (++X_coord >= config.window_width) break;
                }
                if (++Y_coord >= config.window_height) break;
            }
            break;
        }
        break;
    case 0x0E:
        if(chip8->inst.NN==0x9E){
            if(chip8->keypad[chip8->V[chip8->inst.X]]){
                chip8->PC += 2;
            }
        }
        else if(chip8->inst.NN==0xA1){
            if(!chip8->keypad[chip8->V[chip8->inst.X]]){
                chip8->PC += 2;
            }
        }
        break;
    case 0x0F:
        switch (chip8->inst.NN)
        {
        case 0x07:
            chip8->V[chip8->inst.X] = chip8->delay_timer;
            break;
        case 0x0A:
            bool press = false;
            for(uint8_t i=0;i<sizeof(chip8->keypad);i++){
                if(chip8->keypad[i]){
                    chip8->V[chip8->inst.X] = i;
                    press = true;
                    break;
                }
            }
            if(!press){
                chip8->PC -= 2;
            }
            break;
        case 0x15:
            chip8->delay_timer = chip8->V[chip8->inst.X];
            break;
        case 0x18:
            chip8->sound_timer = chip8->V[chip8->inst.X];
            break;
        case 0x1E:
            chip8->I += chip8->V[chip8->inst.X];
            break;
        case 0x29:
            chip8->I = chip8->V[chip8->inst.X] * 5;
            break;
        case 0x33:
            uint8_t dnum = chip8->V[chip8->inst.X];
            chip8->ram[chip8->I+2] = dnum%10;
            dnum /= 10;
            chip8->ram[chip8->I+1] = dnum%10;
            dnum /= 10;
            chip8->ram[chip8->I] = dnum%10;
            break;
        case 0x55:
            for (uint8_t i = 0; i <= chip8->inst.X ; i++){
                chip8->ram[chip8->I+i] = chip8->V[i];
            }
            break;
        case 0x65:
            for (uint8_t i = 0; i <= chip8->inst.X ; i++){
                chip8->V[i]=chip8->ram[chip8->I+i];
            }
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}


void update_timers(chip8_t *chip8,sdl_t sdl){
    if(chip8->delay_timer>0){
        chip8->delay_timer--;
    }
    if(chip8->sound_timer>0){
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl.dev,0);
    }
    else{
        SDL_PauseAudioDevice(sdl.dev,1);
    }
}

int main(int argc, char **argv){
    sdl_t sdl = {0};
    config_t config = {0};
    chip8_t chip8 = {0};
    set_config(&config,argc,argv);
    if(!init_sdl(&sdl,&config)){
        exit(EXIT_FAILURE);
    }
    if(!init_chip8(&chip8,argv[1])){
        exit(EXIT_FAILURE);
    }
    clear_screen(config,sdl);
    srand(time(NULL));

    while(chip8.state!=QUIT){
        handel_input(&chip8);

        if(chip8.state == PAUSED){
            continue;
        }


        uint64_t before = SDL_GetPerformanceCounter();
        for(uint32_t i=0;i<config.inst_per_sec/60;i++){
            emulate_instruction(&chip8,config);
        }
        uint64_t after = SDL_GetPerformanceCounter();

        double time_elapsed = (double)((after-before)*1000)/SDL_GetPerformanceFrequency();

        SDL_Delay(16.67 > time_elapsed ? 16.67-time_elapsed:0);
        update_screen(sdl,config,&chip8);
        update_timers(&chip8,sdl);
    }

    final_sdl(&sdl);
    printf("Closing\n");
}