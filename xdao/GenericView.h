
class GenericView : public Gtk::VBox
{
public:
  virtual void update(unsigned long level) = 0;
};