.. _font_manager:

============
Font Manager
============

Font Manager

.. _font_manager_usage:

Usage
-----

Enable :c:macro:`LV_USE_FONT_MANAGER` in ``lv_conf.h``.

Create Fragment Class
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   struct sample_fragment_t {
       /* IMPORTANT: don't miss this part */
       lv_fragment_t base;
       /* States, object references and data fields for this fragment */
       const char *title;
   };

   const lv_fragment_class_t sample_cls = {
           /* Initialize something needed */
           .constructor_cb = sample_fragment_ctor,
           /* Create view objects */
           .create_obj_cb = sample_fragment_create_obj,
           /* IMPORTANT: size of your fragment struct */
           .instance_size = sizeof(struct sample_fragment_t),
   };

Use ``lv_fragment_manager``
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /* Create fragment instance, and objects will be added to container */
   lv_fragment_manager_t *manager = lv_fragment_manager_create(container, NULL);
   /* Replace current fragment with instance of sample_cls, and init_argument is user defined pointer */
   lv_fragment_manager_replace(manager, &sample_cls, init_argument);

Fragment Based Navigation
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

   /* Add one instance into manager stack. View object of current fragment will be destroyed,
    * but instances created in class constructor will be kept.
    */
   lv_fragment_manager_push(manager, &sample_cls, NULL);

   /* Remove the top most fragment from the stack, and bring back previous one. */
   lv_fragment_manager_pop(manager);

.. _fragment_example:

Example
-------

.. include:: ../examples/others/font_manager/index.rst

.. _fragment_api:

API
---
