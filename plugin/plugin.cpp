/*
 *  This is a sample plugin module
 *
 *  It demonstrates the use of the choose() function
 *
 */
#include <string>
#include <ida.hpp>
#include <loader.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <dbg.hpp>
#include <idd.hpp>


static const std::string g_choose_widget_name = "Choose Module";

static ea_t choose_module_base = 0;

struct item_t {
    ea_t base;
    qstring name;
};

class calls_chooser_t : public chooser_t
{
protected:

    qvector<item_t> m_list;
protected:
    
    static const int widths_[];
    static const char *const header_[];
public:
    calls_chooser_t(const char* title ,qvector<item_t>& items);
    ~calls_chooser_t();

public:
    
    virtual cbret_t idaapi enter(size_t n) override
    {
        msg("Choose Module Name: %s\n", m_list[n].name.c_str());
        msg("Choose Module Base: %x\n", m_list[n].base);
        
        choose_module_base = m_list[n].base;
        return cbret_t();
    }
    
    virtual const void *get_obj_id(size_t *len) const override
    {
        *len = strlen(title);
        return title;
    }
    virtual size_t idaapi get_count() const  override {return m_list.size();}
    virtual void idaapi get_row(qstrvec_t* cols_,
                                int * icon_,
                                chooser_item_attrs_t* attrs,
                                size_t n) const override;
    void push_back(item_t& item_)
    {
        m_list.push_back(item_);
    }
};

// column widths
const int calls_chooser_t::widths_[] =
{
    32,  // Module Name
    16
};

// column headers
const char *const calls_chooser_t::header_[] =
{
    "Name",      // 0
    "Base"
};

inline calls_chooser_t::calls_chooser_t(const char* title_, qvector<item_t>& items_)
    :chooser_t(0,qnumber(widths_),widths_,header_,title_),m_list()
{
    CASSERT(qnumber(widths_) == qnumber(header_));
    
    for (auto& item : items_) {
        m_list.push_back(item);
    }
}

calls_chooser_t::~calls_chooser_t()
{
    
}

void idaapi calls_chooser_t::get_row(qstrvec_t *cols_,
                                     int *icon_,
                                     chooser_item_attrs_t *attrs,
                                     size_t n) const
{
    qstrvec_t& cols = *cols_;
    cols[0] = m_list[n].name;   //第一列列名
    
    char buffer[50] = {0};
    qsnprintf(buffer, MAXSTR, "0x%X",m_list[n].base);
    cols[1] = qstring(buffer);        //第二列列名
}

calls_chooser_t* g_cbt = nullptr;
qvector<item_t> g_module_info;


static idaapi void set_breakpoint()
{
    qstring input_str;
    ask_str(&input_str, HIST_SRCH, "Enter");
    
    ea_t input_ea = 0;
    ea_t screen_ea = 0;
    
    str2ea(&input_ea, input_str.c_str(), screen_ea);
    
    ea_t bpt_addr = choose_module_base + input_ea;
    add_bpt(bpt_addr);  //添加断点
    jumpto(bpt_addr);   //跳转至断点所在位置
}

static bool idaapi ct_keyboard(TWidget*  ,int key, int shift,void* ud)
{
    if (shift == 0) {
        switch (key) {
            case 'Z':
                set_breakpoint();
            return true;
        }
    }
    
    return false;
}

static const custom_viewer_handlers_t handlers(
                                               ct_keyboard,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL);



static ssize_t idaapi view_callback(void * , int notification_code, va_list va)
{
    TWidget* view = va_arg(va,TWidget*);
    
    switch (notification_code) {
        case view_activated:
            set_custom_viewer_handlers(view, &handlers, nullptr);
            break;
        default:
            break;
    }
    return 0;
}

static ssize_t idaapi dbg_callback(void *, int notification_code, va_list va)
{
    switch (notification_code) {
        case dbg_library_load:
        {
            const debug_event_t *pev = va_arg(va, const debug_event_t *);
            item_t temp;
            temp.name = pev->modinfo.name;
            temp.base = pev->modinfo.base;
            g_module_info.push_back(temp);
            if(g_cbt!=nullptr)
            {
                g_cbt->push_back(temp);
                refresh_chooser(g_choose_widget_name.c_str());
                
            }else{
                msg("g_cbt is nullptr\n");
            }
        }
        break;
        
        case dbg_library_unload:
            //TODO...
            break;
            
        default:
            break;
    }
    return 0;
}

/*
   plugin run
*/
bool idaapi run(size_t)
{
    TWidget* wgt = find_widget(g_choose_widget_name.c_str());
    
    if(wgt != nullptr)
    {
        activate_widget(wgt, true);
    }
    else
    {
        module_info_t minfo;
        
        for (bool ok = get_first_module(&minfo); ok; ok = get_next_module(&minfo)) {
            
            //获取模块名和基地址
            item_t temp;
            temp.name = qstring(minfo.name);
            temp.base = minfo.base;
            g_module_info.push_back(temp);
        }
        
        g_cbt= new calls_chooser_t(g_choose_widget_name.c_str(),g_module_info);
        
        g_cbt->choose();
        
        hook_to_notification_point(HT_VIEW, view_callback);
        hook_to_notification_point(HT_DBG, dbg_callback);
    }
    
    return true;
}


/*
   plugin init
*/

int idaapi init(void)
{
  return PLUGIN_OK;
}

/*
    plugin term
 */
void idaapi term(void)
{
    unhook_from_notification_point(HT_VIEW, view_callback);
    unhook_from_notification_point(HT_DBG, dbg_callback);
    
    delete g_cbt;
    g_cbt = nullptr;
}

//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
  IDP_INTERFACE_VERSION,
  // plugin flags
  0,
  // initialize
  init,
  // terminate. this pointer may be NULL.
  term,
  // invoke plugin
  run,
  // long comment about the plugin
  // it could appear in the status line
  // or as a hint
  "This is breakpoint plugin. It choose moudle , input offset",
  // multiline help about the plugin
  "help"
  ""
  "",

  // the preferred short name of the plugin
  "Add Breakpoint With Offset",
  // the preferred hotkey to run the plugin
  ""
};
