// Abstract Class.
// All Childs need to implement this basic funcs!

class GenericChild : public Gnome::MDIChild
{
public:
  virtual void update(unsigned long level) = 0;
  virtual void saveProject() = 0;
  virtual void saveAsProject() = 0;
  virtual bool closeProject() = 0;
  virtual void record_to_cd() = 0;
};