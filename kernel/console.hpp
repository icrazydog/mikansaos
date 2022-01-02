#pragma once

#include <memory>
#include "graphics.hpp"
#include "window.hpp"

class Console{
  public:
    static const int kRows = 25, kColumns = 80;
  
    Console(const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);
    void SetWriter(PixelWriter* writer);
    void SetWindow(const std::shared_ptr<Window>& window);
    void SetLayerID(unsigned int layer_id);
    unsigned int LayerID() const;

  private:
    void NewLine();
    void Refresh();

    PixelWriter* _writer;
    std::shared_ptr<Window> _window;
    const PixelColor _fg_color, _bg_color;
    char _buffer[kRows][kColumns + 1];
    int _cursor_row, _cursor_column;
    unsigned int _layer_id;
};


extern Console* console;
void InitializeConsole();


