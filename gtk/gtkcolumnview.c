/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcolumnview.h"

#include "gtkboxlayout.h"
#include "gtkcolumnviewcolumn.h"
#include "gtkintl.h"
#include "gtklistview.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkcolumnview
 * @title: GtkColumnView
 * @short_description: A widget for displaying lists in multiple columns
 * @see_also: #GtkColumnViewColumn, #GtkTreeView
 *
 * GtkColumnView is a widget to present a view into a large dynamic list of items
 * using multiple columns.
 */

struct _GtkColumnView
{
  GtkWidget parent_instance;

  GListStore *columns;

  GtkListView *listview;
};

struct _GtkColumnViewClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_COLUMNS,
  PROP_MODEL,
  PROP_SHOW_SEPARATORS,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkColumnView, gtk_column_view, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_column_view_activate_cb (GtkListView   *listview,
                             guint          pos,
                             GtkColumnView *self)
{
  g_signal_emit (self, signals[ACTIVATE], 0, pos);
}

static void
gtk_column_view_dispose (GObject *object)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  g_list_store_remove_all (self->columns);

  g_clear_pointer ((GtkWidget **) &self->listview, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_column_view_parent_class)->dispose (object);
}

static void
gtk_column_view_finalize (GObject *object)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  g_object_unref (self->columns);

  G_OBJECT_CLASS (gtk_column_view_parent_class)->finalize (object);
}

static void
gtk_column_view_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  switch (property_id)
    {
    case PROP_COLUMNS:
      g_value_set_object (value, self->columns);
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_view_get_model (self->listview));
      break;

    case PROP_SHOW_SEPARATORS:
      g_value_set_boolean (value, gtk_list_view_get_show_separators (self->listview));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkColumnView *self = GTK_COLUMN_VIEW (object);

  switch (property_id)
    {
    case PROP_MODEL:
      gtk_column_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_SHOW_SEPARATORS:
      gtk_column_view_set_show_separators (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_class_init (GtkColumnViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_dispose;
  gobject_class->finalize = gtk_column_view_finalize;
  gobject_class->get_property = gtk_column_view_get_property;
  gobject_class->set_property = gtk_column_view_set_property;

  /**
   * GtkColumnView:columns:
   *
   * The list of columns
   */
  properties[PROP_COLUMNS] =
    g_param_spec_object ("columns",
                         P_("Columns"),
                         P_("List of columns"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:model:
   *
   * Model for the items displayed
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnView:show-separators:
   *
   * Show separators between rows
   */
  properties[PROP_SHOW_SEPARATORS] =
    g_param_spec_boolean ("show-separators",
                          P_("Show separators"),
                          P_("Show separators between rows"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkColumnView::activate:
   * @self: The #GtkColumnView
   * @position: position of item to activate
   *
   * The ::activate signal is emitted when a row has been activated by the user,
   * usually via activating the GtkListBase|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a columnview.
   * See gtk_list_item_set_activatable() for details on how to use this signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__UINTv);

  gtk_widget_class_set_css_name (widget_class, I_("columnview"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
gtk_column_view_init (GtkColumnView *self)
{
  self->columns = g_list_store_new (GTK_TYPE_COLUMN_VIEW_COLUMN);

  self->listview = GTK_LIST_VIEW (gtk_list_view_new ());
  g_signal_connect (self->listview, "activate", G_CALLBACK (gtk_column_view_activate_cb), self);
  gtk_widget_set_parent (GTK_WIDGET (self->listview), GTK_WIDGET (self));
}

/**
 * gtk_column_view_new:
 *
 * Creates a new empty #GtkColumnView.
 *
 * You most likely want to call gtk_column_view_set_factory() to
 * set up a way to map its items to widgets and gtk_column_view_set_model()
 * to set a model to provide items next.
 *
 * Returns: a new #GtkColumnView
 **/
GtkWidget *
gtk_column_view_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW, NULL);
}

/**
 * gtk_column_view_get_model:
 * @self: a #GtkColumnView
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 **/
GListModel *
gtk_column_view_get_model (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), NULL);

  return gtk_list_view_get_model (self->listview);
}

/**
 * gtk_column_view_set_model:
 * @self: a #GtkColumnView
 * @model: (allow-none) (transfer none): the model to use or %NULL for none
 *
 * Sets the #GListModel to use.
 *
 * If the @model is a #GtkSelectionModel, it is used for managing the selection.
 * Otherwise, @self creates a #GtkSingleSelection for the selection.
 **/
void
gtk_column_view_set_model (GtkColumnView *self,
                           GListModel  *model)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (gtk_list_view_get_model (self->listview) == model)
    return;

  gtk_list_view_set_model (self->listview, model);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_column_view_get_columns:
 * @self: a #GtkColumnView
 *
 * Gets the list of columns in this column view. This list is constant over
 * the lifetime of @self and can be used to monitor changes to the columns
 * of @self by connecting to the GListModel:items-changed signal.
 *
 * Returns: (transfer none): The list managing the columns
 **/
GListModel *
gtk_column_view_get_columns (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), NULL);

  return G_LIST_MODEL (self->columns);
}

/**
 * gtk_column_view_set_show_separators:
 * @self: a #GtkColumnView
 * @show_separators: %TRUE to show separators
 *
 * Sets whether the list should show separators
 * between rows.
 */
void
gtk_column_view_set_show_separators (GtkColumnView *self,
                                     gboolean     show_separators)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW (self));

  if (gtk_list_view_get_show_separators (self->listview) == show_separators)
    return;

  gtk_list_view_set_show_separators (self->listview, show_separators);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEPARATORS]);
}

/**
 * gtk_column_view_get_show_separators:
 * @self: a #GtkColumnView
 *
 * Returns whether the list box should show separators
 * between rows.
 *
 * Returns: %TRUE if the list box shows separators
 */
gboolean
gtk_column_view_get_show_separators (GtkColumnView *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW (self), FALSE);

  return gtk_list_view_get_show_separators (self->listview);
}