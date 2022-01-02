/**
 *@file graphics.cpp
 *
 * graphics process
 */

#include "graphics.hpp"

#include "logger.hpp"
#include "font.hpp"

void RGBResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c){
  auto p = PixelAt(pos);
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
}

PixelColor RGBResv8BitPerColorPixelWriter::GetPixel(Vector2D<int> pos){
  PixelColor c;
  auto p = PixelAt(pos);
  c.r = p[0];
  c.g = p[1];
  c.b = p[2];
  return c;
}

void BGRResv8BitPerColorPixelWriter::Write(Vector2D<int> pos, const PixelColor& c){
  auto p = PixelAt(pos);
  p[0] = c.b;
  p[1] = c.g;
  p[2] = c.r;
}

PixelColor BGRResv8BitPerColorPixelWriter::GetPixel(Vector2D<int> pos){
  PixelColor c;
  auto p = PixelAt(pos);
  c.b = p[0];
  c.g = p[1];
  c.r = p[2];
  return c;
}



void DrawRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c){
  for(int dx = 0; dx < size.x; dx++){
    writer.Write(pos + Vector2D<int>{dx, 0}, c);
    writer.Write(pos + Vector2D<int>{dx, size.y - 1} , c);
  }
  for(int dy = 1; dy < size.y -1 ; dy++){
    writer.Write(pos + Vector2D<int>{0, dy},c );
    writer.Write(pos + Vector2D<int>{size.x - 1, dy}, c);
  }
}

void FillRectangle(PixelWriter& writer, const Vector2D<int>& pos,
                   const Vector2D<int>& size, const PixelColor& c){
  for(int dy = 0; dy < size.y; dy++){
    for(int dx = 0; dx < size.x; dx++){
      writer.Write(pos + Vector2D<int>{dx, dy}, c);
    }
  }
}

void DrawCircle(PixelWriter& writer, const Vector2D<int>& pos,
                   int radius,int lineWidth, const PixelColor& c){
  for(int lw = 1; lw<=lineWidth; lw++){
    radius = radius + lw - 1;
    int r2 = radius * radius;
    writer.Write(pos +  Vector2D<int>{0, radius}, c);
    writer.Write(pos +  Vector2D<int>{0, -radius}, c);
    writer.Write(pos +  Vector2D<int>{radius, 0}, c);
    writer.Write(pos +  Vector2D<int>{-radius, 0}, c);
  
    int dx = 1;
    int dy = (int)(std::sqrt(r2 - dx*dx) + 0.0);
    
    while(dx < dy){
        writer.Write(Vector2D<int>{pos.x + dx, pos.y + dy}, c);
        writer.Write(Vector2D<int>{pos.x + dx, pos.y - dy}, c);
        writer.Write(Vector2D<int>{pos.x - dx, pos.y + dy}, c);
        writer.Write(Vector2D<int>{pos.x - dx, pos.y - dy}, c);
        writer.Write(Vector2D<int>{pos.x + dy, pos.y + dx}, c);
        writer.Write(Vector2D<int>{pos.x + dy, pos.y - dx}, c);
        writer.Write(Vector2D<int>{pos.x - dy, pos.y + dx}, c);
        writer.Write(Vector2D<int>{pos.x - dy, pos.y - dx}, c);
  
        dx += 1;
        dy = (int)(std::sqrt(r2 - dx*dx) + 0.0);
    }
  
    if(dx==dy){
      writer.Write(pos + Vector2D<int>{dx, dy}, c);
      writer.Write(Vector2D<int>{pos.x + dx, pos.y - dy}, c);
      writer.Write(Vector2D<int>{pos.x - dx, pos.y + dy}, c);
      writer.Write(Vector2D<int>{pos.x - dx, pos.y - dy}, c);
    }
  }
}

void FillCircle(PixelWriter& writer, const Vector2D<int>& pos,
                   const int radius, const PixelColor& c){
  int r2 = radius * radius;
  for(int dx = -radius; dx <= radius; dx++){
      int h = (int)(std::sqrt(r2 - dx*dx) + 0.5);
      for(int dy = -h; dy <= h; dy++){
        writer.Write(pos + Vector2D<int>{dx, dy}, c);
      }
  }
}

void FillPolygon(PixelWriter& pixel_writer, const std::vector<Vector2D<int>>& vertexs, const PixelColor& c){
  const int kCavasWidth = pixel_writer.Width();
  const int kCavasHeight = pixel_writer.Height();
  
  std::vector<int> pixelXOnPolygon;
  for(int pixelY=0; pixelY<kCavasHeight; pixelY++){
    pixelXOnPolygon.clear();
    for(int i=0; i<vertexs.size(); i++){
      int j = i + 1;
      if(j>=vertexs.size()){
        j = 0;
      }
      if((vertexs[i].y >= pixelY && vertexs[j].y < pixelY) ||
          (vertexs[i].y <= pixelY && vertexs[j].y > pixelY)){
        //edge cross the line on pixelY
        int crossX = vertexs[i].x + 
            (vertexs[i].y - pixelY)*1.0f/(vertexs[i].y - vertexs[j].y)*
            (vertexs[j].x - vertexs[i].x);
        // Log(kDebug, "crossX:%d    %d,%d ---  %d,%d\n",
        //     crossX,
        //     vertexs[i].x, vertexs[i].y,
        //     vertexs[j].x ,  vertexs[j].y);
        pixelXOnPolygon.push_back(crossX);
      }
    }
    if(pixelXOnPolygon.size()==0){
      continue;
    }
    //Log(kDebug, "pixelXOnPolygon: %d\n",pixelXOnPolygon.size());

    std::sort(pixelXOnPolygon.begin(), pixelXOnPolygon.end() );
    
    for(int i=0; i<pixelXOnPolygon.size(); i+=2){
      int endIndex = i+1;
      if(endIndex>=pixelXOnPolygon.size()){
        endIndex = i;
      }
      for(int crossX=pixelXOnPolygon[i];
            crossX<=pixelXOnPolygon[endIndex];
            crossX++){
        int pixelX = crossX;
        if(pixelX<0){
          pixelX = 0;
        }
        if(pixelX>=kCavasWidth){
          pixelX = kCavasWidth - 1;
        }
        pixel_writer.Write(Vector2D<int>{pixelX, pixelY}, c);
      }
    }
  }
}

void DrawDesktop(PixelWriter& pixel_writer) {

  const int kFrameWidth = pixel_writer.Width();
  const int kFrameHeight = pixel_writer.Height();

  FillRectangle(pixel_writer,{0, 0},{kFrameWidth, kFrameHeight}, kDesktopBGColor);


}

void DrawTaskBar(PixelWriter& pixel_writer) {
  const int kFrameWidth = pixel_writer.Width();
  const int kFrameHeight = pixel_writer.Height();

  const int distanceFromCenter = 100;



  FillRectangle(pixel_writer,{0, kFrameHeight - kTaskBarHeight},
      {kFrameWidth/2-distanceFromCenter, kTaskBarHeight}, kTaskBarColor);

  FillPolygon(pixel_writer,
    {{kFrameWidth/2-distanceFromCenter, kFrameHeight - kTaskBarHeight},
      {kFrameWidth/2-distanceFromCenter, kFrameHeight},
      {kFrameWidth/2 -distanceFromCenter + kTaskBarHeight , kFrameHeight}}, kTaskBarColor);

  FillRectangle(pixel_writer,{kFrameWidth/2 + distanceFromCenter, kFrameHeight - kTaskBarHeight},
      {kFrameWidth/2 -distanceFromCenter, kTaskBarHeight}, kTaskBarColor);
  
  FillPolygon(pixel_writer,
    {{kFrameWidth/2  +distanceFromCenter , kFrameHeight - kTaskBarHeight},
      {kFrameWidth/2 +distanceFromCenter, kFrameHeight},
      {kFrameWidth/2 +distanceFromCenter - kTaskBarHeight, kFrameHeight}}, kTaskBarColor);

  FillCircle(pixel_writer,{kFrameWidth/2, kFrameHeight - kInfoBallRadius -5}, kInfoBallRadius ,kMainLightColor);
  
  //date and time
  WriteString(pixel_writer, {kFrameWidth/2 -20, kFrameHeight - kInfoBallRadius -5 -8}, "15:10", kDesktopFGColor);
  WriteString(pixel_writer, {kFrameWidth-80-20, kFrameHeight - kTaskBarHeight/2 -8}, "2021-11-20", kMainTextColor);
}


void DrawDesktopButton(PixelWriter& pixel_writer) {
  FillCircle(pixel_writer,{23, 23}, 15,{160, 160, 160});
  DrawCircle(pixel_writer,{23, 23}, 21, 2,{160, 160, 160});
}


namespace {
  char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
  Vector2D<int> screen_size;
}
FrameBufferConfig screen_frame_buffer_config;
PixelWriter* screen_pixel_writer;

Vector2D<int> ScreenSize() {
  return screen_size;
}


void InitializeGraphics(const FrameBufferConfig& screen_config){
  ::screen_frame_buffer_config = screen_config;

  screen_size.x = screen_frame_buffer_config.horizontal_resolution;
  screen_size.y = screen_frame_buffer_config.vertical_resolution;

  switch (screen_frame_buffer_config.pixel_format){
    case kPixelRGBResv8BitPerColor:
      screen_pixel_writer = new(pixel_writer_buf) 
        RGBResv8BitPerColorPixelWriter{screen_frame_buffer_config};
      break;
    case kPixelBGRResv8BitPerColor:
      screen_pixel_writer = new(pixel_writer_buf) 
        BGRResv8BitPerColorPixelWriter{screen_frame_buffer_config};
      break;
    default:
      exit(1);
  }

  DrawDesktop(*screen_pixel_writer);
}