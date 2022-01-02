#pragma once

#include <memory>
#include <functional>
#include <map>

#include "window.hpp"
#include "message.hpp"

class Layer{
  public:
    Layer(unsigned int id = 0);
    unsigned int ID() const;

    Layer& SetWindow(const std::shared_ptr<Window>& window);
    std::shared_ptr<Window> GetWindow() const;

    Vector2D<int> GetPosition() const;
    Layer& SetDraggable(bool draggable);
    bool IsDraggable() const;
    Layer& SetTransparentable(bool transparentable);
    bool IsTransparentable() const;

    Layer& Move(Vector2D<int> pos);
    Layer& MoveRelative(Vector2D<int> pos_delta);

    void DrawTo(FrameBuffer& screen,  const Rectangle<int>& area, bool transparent) const;

    Layer&  SetOnClick(std::function<void (uint8_t)> handler);
    void OnClick(uint8_t button_id);
  private:
    unsigned int _id;
    Vector2D<int> _pos;
    std::shared_ptr<Window> _window;
    bool _draggable{false};
    bool _transparentable {false};
    std::function<void (uint8_t)> _onClickHandler;
};

class LayerManager{
  public:
    void SetWriter(FrameBuffer* writer);
    Layer& NewLayer();
    void RemoveLayer(unsigned int id);
    
    void Draw(const Rectangle<int>& area) const;
    void Draw(unsigned int id) const;
    void Draw(unsigned int id, Rectangle<int> area) const;
    void Fresh() const;

    void Move(unsigned int id, Vector2D<int> pos);
    void MoveRelative(unsigned int id, Vector2D<int> pos_delta);
  
    //index is the stack order of layer, greater index at front
    void SetIndex(unsigned int id, int index);
    void Hide(unsigned int id);

    Layer* FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const;
    Layer* FindLayer(unsigned int id);
    int GetIndex(unsigned int id);


  private:
    FrameBuffer* _screen{nullptr};
    mutable FrameBuffer _back_buffer{};
    std::vector<std::unique_ptr<Layer>> _layers{};
    std::vector<Layer*> _layer_stack{};
    unsigned int _last_id{0};
};

extern LayerManager* layer_manager;

class ActiveLayer {
 public:
  ActiveLayer(LayerManager& manager);
  void SetMouseLayer(unsigned int mouse_layer);
  void Activate(unsigned int layer_id);
  unsigned int GetActive() const { return _active_layer; }

 private:
  LayerManager& _manager;
  unsigned int _active_layer{0};
  unsigned int _mouse_layer{0};
};

extern ActiveLayer* active_layer;
extern std::map<unsigned int, uint64_t>* layer_task_map;

void InitializeLayer();
void ProcessLayerMessage(const Message& msg);

constexpr Message MakeLayerMessage(
    uint64_t task_id, unsigned int layer_id,
    LayerOperation op, const Rectangle<int>& area) {
  Message msg{Message::kLayerOps, task_id};
  msg.arg.layer.layer_id = layer_id;
  msg.arg.layer.op = op;
  msg.arg.layer.x = area.pos.x;
  msg.arg.layer.y = area.pos.y;
  msg.arg.layer.w = area.size.x;
  msg.arg.layer.h = area.size.y;
  return msg;
}

Error CloseLayer(unsigned int layer_id);