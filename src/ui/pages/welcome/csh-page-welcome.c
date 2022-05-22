#include "csh-page-welcome.h"


struct _CshPageWelcome
{
  AdwBin parent_instance;
};

G_DEFINE_TYPE (CshPageWelcome, csh_page_welcome, ADW_TYPE_BIN)

static void
csh_page_welcome_class_init (CshPageWelcomeClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/spaceinventor/csh/pages/welcome/csh-page-welcome.ui");
}

static void
csh_page_welcome_init (CshPageWelcome *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
