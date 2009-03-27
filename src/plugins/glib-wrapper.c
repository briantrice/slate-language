#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>

EXPORT gint wrapper_g_value_type( GValue *gValueOop ) {
	return G_VALUE_TYPE(gValueOop);
}

EXPORT gint wrapper_g_type_fundamental( gint gType ) {
	return G_TYPE_FUNDAMENTAL(gType);
}

EXPORT gboolean wrapper_g_type_is_fundamental( gint gType ) {
	return G_TYPE_IS_FUNDAMENTAL(gType);
}

EXPORT GValue *wrapper_g_value_new(void) {
	return g_new0(GValue, 1);
}

EXPORT GType wrapper_g_type_from_instance( GObject *object ) {
	return G_TYPE_FROM_INSTANCE( object );
}

GAsyncQueue* callbackQueue;

EXPORT void wrapper_g_callback_queue_init( void ) {
	callbackQueue = g_async_queue_new();
}

EXPORT void wrapper_g_callback_queue_shutdown( void ) {
	g_async_queue_unref( callbackQueue );
}

typedef struct{
	gint blockID;
	gint parameterCount;
	GValue *parameters[];
} CallbackData;

EXPORT CallbackData *wrapper_g_callback_wait_next(void) {
	return (CallbackData *)g_async_queue_pop(callbackQueue);
}

EXPORT void wrapper_g_callback_end( CallbackData* data ) {
	int index;
	for(index = 0; index < data->parameterCount; index++) {
		g_value_unset( data->parameters[index] );
		g_free( data->parameters[index] );
	}
	g_free(data);
}

EXPORT gint wrapper_g_callback_data_get_block_id( CallbackData* data ) {
	return data->blockID;
}

EXPORT gint wrapper_g_callback_data_get_parameter_count( CallbackData* data ) {
	return data->parameterCount;
}

EXPORT GValue *wrapper_g_callback_data_get_parameter_at( CallbackData* data, gint index ) {
	return data->parameters[index];
}

static gboolean callback(GObject* object, gpointer blockID, gint parameterCount, const GValue* parameters) {
	CallbackData *data = g_malloc0( sizeof(CallbackData) + sizeof(GValue *) * (parameterCount + 1) );
	data->blockID = (gint)blockID;
	data->parameterCount = parameterCount + 1;
	//Copy the emitter as the first argument
	data->parameters[0] = wrapper_g_value_new();
	g_value_init( data->parameters[0], G_TYPE_OBJECT );
	g_value_set_object( data->parameters[0], object );
	int index;
	for(index = 0; index < parameterCount; index++) {
		//We copy the parameters because we may return from the callback before even using this values, the callback generator destorys the data on return
		data->parameters[index + 1] = wrapper_g_value_new();
		g_value_init( data->parameters[index + 1], G_VALUE_TYPE((GValue *)parameters + index) );
		g_value_copy( (GValue *)parameters + index, data->parameters[index + 1] );
	}
	g_async_queue_push( callbackQueue, data );
	return TRUE;
}

void callback_marshal (GClosure *closure, GValue *return_value, guint n_param_values,
							const GValue *param_values, gpointer invocation_hint, gpointer marshal_data) {
	
	typedef gboolean (*SignalCallbackFunc) (GObject *object, gpointer data, gint paramValuesSize, const GValue *paramValues);
	register SignalCallbackFunc callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;
	gboolean retVal;

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}

	callback = (SignalCallbackFunc) (marshal_data ? marshal_data : cc->callback);
	retVal = callback (data1, data2, (gint) (n_param_values - 1), param_values + 1);
	if (return_value)
		g_value_set_boolean (return_value, retVal);
}

void callback_finalize_notifier( gpointer blockID, GClosure *closure ) {
//callbacks with parameter count=0 signal the finalization of a callback, which means that it is no longer valid, ie: the object was destroyed
	CallbackData *data = g_malloc0( sizeof(CallbackData) );
	data->blockID = (gint)blockID;
	data->parameterCount = 0;
	g_async_queue_push( callbackQueue, data );
}
/*
EXPORT void wrapper_g_object_connect_to_block_id( gpointer instance, char* aSignalName, gint blockID ) {
	GClosure *closure = g_cclosure_new( G_CALLBACK(callback), (gpointer)blockID, NULL);
	g_closure_set_marshal( closure, callback_marshal);
	g_closure_add_finalize_notifier( closure, (gpointer)blockID, callback_finalize_notifier );
	g_signal_connect_closure(instance, aSignalName, closure, FALSE);
}
*/
EXPORT GClosure *wrapper_g_cclosure_new(gint blockID) {
	GClosure *closure = g_cclosure_new( G_CALLBACK(callback), (gpointer)blockID, callback_finalize_notifier);
	g_closure_set_marshal( closure, callback_marshal);
	return closure;
}

EXPORT gchar *wraper_g_pointer_as_string( gpointer *pointer ) {
	return (gchar *)pointer;
}