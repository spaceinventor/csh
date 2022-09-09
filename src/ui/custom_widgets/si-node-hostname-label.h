#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define SI_NODE_TYPE_HOSTNAME_LABEL (si_node_hostname_label_get_type())

extern G_DECLARE_FINAL_TYPE(SiNodeHostnameLabel, si_node_hostname_label, SI, NODE_HOSTNAME_LABEL, GtkWidget)

G_END_DECLS
