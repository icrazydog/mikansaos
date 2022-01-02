#include "layer.hpp"

#include <algorithm>

#include "logger.hpp"
#include "timer.hpp"
#include "task.hpp"

namespace {
  template <class T, class U>
  void EraseIf(T& c, const U& pred) {
    auto iter = std::remove_if(c.begin(), c.end(), pred);
    c.erase(iter, c.end());
  }
}

Layer::Layer(unsigned int id) : _id{id}{

}
unsigned int Layer::ID() const{
  return _id;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window){
  _window = window;
  return *this;
}

std::shared_ptr<Window> Layer::GetWindow() const{
  return _window;
}

Vector2D<int> Layer::GetPosition() const {
  return _pos;
}

Layer& Layer::SetDraggable(bool draggable) {
  _draggable = draggable;
  return *this;
}

bool Layer::IsDraggable() const {
  return _draggable;
}

Layer& Layer::SetTransparentable(bool transparentable){
  _transparentable = transparentable;
  return *this;
}

bool Layer::IsTransparentable() const{
  return _transparentable;
}

Layer& Layer::Move(Vector2D<int> pos){
  _pos = pos;
  return *this;
}
Layer& Layer::MoveRelative(Vector2D<int> pos_delta){
  _pos += pos_delta;
  return *this;
}

void Layer::DrawTo(FrameBuffer& writer,  const Rectangle<int>& area, bool transparent) const{
  if(_window){
    _window->DrawTo(writer, _pos, area, transparent);
  }
}

Layer&  Layer::SetOnClick(std::function<void (uint8_t)> handler){
  _onClickHandler = handler;
   return *this;
}

void Layer::OnClick(uint8_t button_id){
  if(_onClickHandler){
    _onClickHandler(button_id);
  }
}


void LayerManager::SetWriter(FrameBuffer* writer){
  _screen = writer;
  FrameBufferConfig back_config = _screen->Config();
  back_config.frame_buffer = nullptr;
  _back_buffer.Initialize(back_config);
}
Layer& LayerManager::NewLayer(){
  return *_layers.emplace_back(new Layer{++_last_id});
}

void LayerManager::RemoveLayer(unsigned int id){
  Hide(id);

  auto pred = [id](const std::unique_ptr<Layer>& elem) {
    return elem->ID() == id;
  };
  EraseIf(_layers, pred);
}

void LayerManager::Draw(const Rectangle<int>& area) const{
  for(auto layer : _layer_stack){
    layer->DrawTo(_back_buffer, area, 
        layer->IsTransparentable() && globalTransparent!=0xff);
  }
  _screen->Copy(area.pos, _back_buffer, area);
}

void LayerManager::Draw(unsigned int id) const{
  Draw(id, {{0, 0}, {-1, -1}});
}

void LayerManager::Draw(unsigned int id, Rectangle<int> area) const{
  bool draw = false;
  bool transparent = false;
  Rectangle<int> window_area;
  for (auto layer : _layer_stack) {
    if (layer->ID() == id) {
      window_area.size = layer->GetWindow()->Size();
      window_area.pos = layer->GetPosition();
      if (area.size.x >= 0 || area.size.y >= 0) {
        area.pos = area.pos + window_area.pos;
        window_area = window_area & area;
      }
      draw = true;
      if(layer->IsTransparentable() && globalTransparent!=0xff){
        transparent = true;
        break;
      }
    }
    if (draw) {
       if (layer->ID() != id){
        Rectangle<int> temp_area;
        temp_area.size = layer->GetWindow()->Size();
        temp_area.pos = layer->GetPosition();
        auto intersection_area=window_area & temp_area;
        if(intersection_area.size.x==0 || intersection_area.size.y==0){
          continue;
        }
      }
      layer->DrawTo(_back_buffer, window_area,
          layer->IsTransparentable() && globalTransparent!=0xff);
    }
  }

  if(transparent){
    Draw(window_area);
  }else{
    _screen->Copy(window_area.pos, _back_buffer, window_area);
  }
}

void LayerManager::Fresh() const{
  for(auto layer : _layer_stack){
    Rectangle<int> temp_area;
    temp_area.size = ScreenSize();
    temp_area.pos = {0 ,0};

    layer->DrawTo(_back_buffer, temp_area, 
    layer->IsTransparentable() && globalTransparent!=0xff);
  }
  _screen->Copy({0 ,0}, _back_buffer, {{0 ,0},ScreenSize()});
}

void LayerManager::Move(unsigned int id, Vector2D<int> pos){
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->Move(pos);
  Draw({old_pos, window_size});
  Draw(id);
}
void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_delta){
  auto layer = FindLayer(id);
  const auto window_size = layer->GetWindow()->Size();
  const auto old_pos = layer->GetPosition();
  layer->MoveRelative(pos_delta);
  Draw({old_pos, window_size});
  Draw(id);
}

void LayerManager::SetIndex(unsigned int id, int index){
  if(index<0){
    Hide(index);
    return;
  }
  if(index > _layer_stack.size()){
    index = _layer_stack.size();
  }

  auto layer = FindLayer(id);
  auto iter_old_pos = std::find(_layer_stack.begin(), _layer_stack.end(), layer);
  auto iter_new_pos = _layer_stack.begin() + index;

  if( iter_old_pos != _layer_stack.end()){
    //finded, adjust index in stack

    _layer_stack.erase(iter_old_pos);
    if(iter_new_pos >= _layer_stack.end()){
      iter_new_pos = _layer_stack.end()-1;
    }
    _layer_stack.insert(iter_new_pos, layer);

  }else{
    //not find, insert new to stack
    _layer_stack.insert(iter_new_pos, layer);
  }
}

void LayerManager::Hide(unsigned int id){
  auto layer = FindLayer(id);
  auto iter = std::find(_layer_stack.begin(), _layer_stack.end(), layer);
  if(iter != _layer_stack.end()){
    _layer_stack.erase(iter);
  }
}

Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const{
  auto pred = [pos, exclude_id](Layer* layer) {
    if (layer->ID() == exclude_id) {
      return false;
    }
    const auto& win = layer->GetWindow();
    if (!win) {
      return false;
    }
    const auto win_pos = layer->GetPosition();
    const auto win_end_pos = win_pos + win->Size();
    return win_pos.x <= pos.x && pos.x < win_end_pos.x &&
           win_pos.y <= pos.y && pos.y < win_end_pos.y;
  };
  auto it = std::find_if(_layer_stack.rbegin(), _layer_stack.rend(), pred);
  if (it == _layer_stack.rend()) {
    return nullptr;
  }
  return *it;
}

Layer* LayerManager::FindLayer(unsigned int id){
  auto pred = [id](const std::unique_ptr<Layer>& elem){
    return elem->ID() == id;
  };

  auto iter = std::find_if(_layers.begin(), _layers.end(), pred);
  if(iter != _layers.end()){
    return iter->get();
  }

  return nullptr;
}

int LayerManager::GetIndex(unsigned int id){
    for (int i = 0; i < _layer_stack.size(); i++) {
    if (_layer_stack[i]->ID() == id) {
      return i;
    }
  }
  return -1;
}

namespace {
    FrameBuffer* screen;

    Error SendWindowActiveMessage(unsigned int layer_id, int activate) {
      auto task_iter = layer_task_map->find(layer_id);
      if (task_iter == layer_task_map->end()) {
        return MAKE_ERROR(Error::kNoSuchTask);
      }

      Message msg{Message::kWindowActive};
      msg.arg.window_active.activate = activate;
      return task_manager->SendMessage(task_iter->second, msg);
    }
}
LayerManager* layer_manager;


ActiveLayer::ActiveLayer(LayerManager& manager):_manager{manager}{

}
void ActiveLayer::SetMouseLayer(unsigned int mouse_layer){
  _mouse_layer = mouse_layer;
}

void ActiveLayer::Activate(unsigned int layer_id){
  if (_active_layer == layer_id) {
    return;
  }

  if (_active_layer > 0) {
    Layer* layer = _manager.FindLayer(_active_layer);
    layer->GetWindow()->Deactivate();
    _manager.Draw(_active_layer);
    SendWindowActiveMessage(_active_layer, 0);
  }

  _active_layer = layer_id;
  if (_active_layer > 0) {
    Layer* layer = _manager.FindLayer(_active_layer);
    layer->GetWindow()->Activate();
    _manager.SetIndex(_active_layer, 0);
    _manager.SetIndex(_active_layer, _manager.GetIndex(_mouse_layer) - 1);
    _manager.Draw(_active_layer);
    SendWindowActiveMessage(_active_layer, 1);
  }
}

ActiveLayer* active_layer;
std::map<unsigned int, uint64_t>* layer_task_map;
 

void InitializeLayer() {
  //init desktop
  //draw window
  const auto screen_size = ScreenSize();

  auto bg_window = std::make_shared<Window>(
      screen_size.x, screen_size.y, screen_frame_buffer_config.pixel_format);
  auto bgwriter = bg_window->Writer();
  DrawDesktop(*bgwriter);
  DrawTaskBar(*bgwriter);



  auto bg_button_window = std::make_shared<Window>(
      45, 45, screen_frame_buffer_config.pixel_format);
  bg_button_window->SetTransparentColor(PixelColor{0, 0, 0});
  auto bgButtonWriter = bg_button_window->Writer();
  DrawDesktopButton(*bgButtonWriter);


  auto console_window = std::make_shared<Window>(
    Console::kColumns * 8, Console::kRows * 16, screen_frame_buffer_config.pixel_format);
  // console->SetWriter(bgwriter);
  console->SetWindow(console_window);

  //draw screen
  screen = new FrameBuffer;
  if (auto err = screen->Initialize(screen_frame_buffer_config)) {
    Log(kError, err, "failed to initialize frame buffer: %s at %s:%d\n",
        err.Name());
  }


  layer_manager = new LayerManager;
  layer_manager->SetWriter(screen);

  auto bglayer_id = layer_manager->NewLayer()
      .SetWindow(bg_window)
      .Move({0, 0})
      .ID();

  auto bg_button_layer_id = layer_manager->NewLayer()
      .SetWindow(bg_button_window)
      .SetOnClick([](uint8_t bution){ 
        if(globalTransparent==0xff){
          Log(kInfo,"transparent ON\n");
          globalTransparent = globalTransparentDefaultAplha;
        }else{
          Log(kInfo,"transparent OFF\n");
          globalTransparent = 0xff;
        }
        layer_manager->Fresh();
      })
      .Move({15,  screen_size.y - 121})
      .ID();

  console->SetLayerID(layer_manager->NewLayer()
      .SetWindow(console_window)
      .Move({0, 0})
      .ID());

  layer_manager->SetIndex(bglayer_id, 0);
  layer_manager->SetIndex(console->LayerID(), 1);
  layer_manager->SetIndex(bg_button_layer_id, 2);

   active_layer = new ActiveLayer{*layer_manager};
   layer_task_map = new std::map<unsigned int, uint64_t>;
}


void ProcessLayerMessage(const Message& msg){
  const auto& msg_params = msg.arg.layer;
    switch (msg_params.op){
    case LayerOperation::Move:
      layer_manager->Move(msg_params.layer_id, 
          {msg_params.x, msg_params.y});
      break;
    case LayerOperation::MoveRelative:
      layer_manager->MoveRelative(msg_params.layer_id, 
          {msg_params.x, msg_params.y});
      break;
    case LayerOperation::Draw:
      if(active_layer->GetActive() == msg_params.layer_id){
        auto elapsed = LAPICTimerElapsed();
        layer_manager->Draw(msg_params.layer_id);
        elapsed = LAPICTimerElapsed() - elapsed;
        Log(kDebug,"draw layer %u [%-8u]\n", msg_params.layer_id, elapsed);
      }else{
        layer_manager->Draw(msg_params.layer_id);
      }
      break;
    case LayerOperation::DrawArea:
      if(active_layer->GetActive() == msg_params.layer_id){
        auto elapsed = LAPICTimerElapsed();
        layer_manager->Draw(msg_params.layer_id, 
            {{msg_params.x, msg_params.y}, {msg_params.w, msg_params.h}});
        elapsed = LAPICTimerElapsed() - elapsed;
        Log(kDebug,"draw area layer %u [%08u]\n", msg_params.layer_id, elapsed);
      }else{
        layer_manager->Draw(msg_params.layer_id, 
            {{msg_params.x, msg_params.y}, {msg_params.w, msg_params.h}});
      }
    break;
    default:
      break;
  }
}

Error CloseLayer(unsigned int layer_id){
  const auto layer = layer_manager->FindLayer(layer_id);

  if (layer == nullptr) {
    return MAKE_ERROR(Error::kNoSuchEntry);
  }

  const auto layer_pos = layer->GetPosition();
  const auto win_size = layer->GetWindow()->Size();

  __asm__("cli");
  active_layer->Activate(0);
  layer_manager->RemoveLayer(layer_id);
  layer_manager->Draw({layer_pos, win_size});
  layer_task_map->erase(layer_id);
  __asm__("sti");

  return MAKE_ERROR(Error::kSuccess);
}