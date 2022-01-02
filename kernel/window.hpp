#pragma once

#include <optional>
#include <vector>
#include <string>

#include "graphics.hpp"
#include "frame_buffer.hpp"

 
template<class P, class M>
P* _container_of_impl(M* ptr, const M P::*member) {
    return (P*)( (char*)ptr - ((size_t) &(reinterpret_cast<P*>(0)->*member)));
}

#define container_of(ptr, type, member) \
     _container_of_impl (ptr, &type::member)


const static uint8_t globalTransparentDefaultAplha = 200;
extern uint8_t globalTransparent;


enum class WindowRegion {
  kTitleBar,
  kCloseButton,
  kBorder,
  kOther,
};

class Window{
  public:
    class WindowWriter: public PixelWriter{
      public:

        virtual void Write(Vector2D<int> pos, const PixelColor& c) override{
          Window* _window =container_of(this, Window, _writer);
          _window->Write(pos, c);
        }

        virtual PixelColor GetPixel(Vector2D<int> pos) override{
          Window* _window =container_of(this, Window, _writer);
          return _window->At(pos);
        }
        
        virtual int Width() override { 
          Window* _window = container_of(this, Window, _writer);
          return _window->Width(); 
        }
        virtual int Height() override { 
           Window* _window = container_of(this, Window, _writer);
          return _window->Height(); 
          }
    };

    Window(int width, int height, PixelFormat shadow_format);
    virtual ~Window() = default;
    Window(const Window& rhs) = delete;
    Window& operator=(const Window& rhs) = delete;

    void DrawTo(FrameBuffer& writer, Vector2D<int> pos, const Rectangle<int>& area, bool transparent);
    void SetTransparentColor(std::optional<PixelColor> c);
    WindowWriter* Writer();
    const PixelColor& At(Vector2D<int> pos) const;
    void Write(Vector2D<int> pos, PixelColor c);

    int Width() const;
    int Height() const;
    Vector2D<int> Size() const;

    void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

    virtual void Activate() {}
    virtual void Deactivate() {}
    virtual WindowRegion GetWindowRegion(Vector2D<int> pos);

  private:
    int _width, _height;
    std::vector<std::vector<PixelColor>> _data{};
    WindowWriter _writer{};
    std::optional<PixelColor> _transparent_color{std::nullopt};
    FrameBuffer _shadow_buffer{};
};


class ToplevelWindow : public Window {
 public:
  static constexpr Vector2D<int> kTopLeftMargin{2, 27};
  static constexpr Vector2D<int> kBottomRightMargin{2, 2};
  static constexpr int kMarginX = kTopLeftMargin.x + kBottomRightMargin.x;
  static constexpr int kMarginY = kTopLeftMargin.y + kBottomRightMargin.y;

  class InnerAreaWriter : public PixelWriter {
   public:
    InnerAreaWriter(ToplevelWindow& window) : _window{window} {}
    virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
      _window.Write(pos + kTopLeftMargin, c);
    }

    virtual PixelColor GetPixel(Vector2D<int> pos) override{
      return _window.At(pos+kTopLeftMargin);
    }

    virtual int Width() override {
      return _window.Width() - kTopLeftMargin.x - kBottomRightMargin.x; }
    virtual int Height() override {
      return _window.Height() - kTopLeftMargin.y - kBottomRightMargin.y; }

   private:
    ToplevelWindow& _window;
  };

  ToplevelWindow(int width, int height, PixelFormat shadow_format,
                 const std::string& title);

  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual WindowRegion GetWindowRegion(Vector2D<int> pos) override;

  InnerAreaWriter* InnerWriter() { return &_inner_writer; }
  Vector2D<int> InnerSize() const;

 private:
  std::string _title;
  InnerAreaWriter _inner_writer{*this};
};

void DrawWindow(PixelWriter& writer, const char* title);
void DrawTextbox(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size);
void DrawTerminal(PixelWriter& writer, Vector2D<int> pos, Vector2D<int> size);
void DrawWindowTitle(PixelWriter& writer, const char* title, bool active);