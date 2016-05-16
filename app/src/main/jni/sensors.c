#include "sensors.h"

/*
 *
 * Python extension to get a list of sensor info
 * for each available sensor.
 *
 * Returns
 *  [{"name" : <name>, "vendor":<vendor>, ...},..]
 *
 * TODO:
 *      call rest of Android Sensor info methods
 */

PyObject* sensor_get_sensor_list(PyObject *self) {

    JNIEnv* jni_env;
    jclass sensor_service_class;
    jmethodID sensor_service_getter;
    jobject sensor_service_object;
    jmethodID sensor_service_method;

    // Use the cached JVM pointer to get a new environment
    (*cached_vm)->AttachCurrentThread(cached_vm, &jni_env, NULL);

    // Find SensorService class and get singleton instance
    sensor_service_class = (*jni_env)->FindClass(jni_env, "com/snakei/SensorService");
    sensor_service_getter = (*jni_env)->GetStaticMethodID(jni_env, sensor_service_class, "getInstance", "()Lcom/snakei/SensorService;");
    sensor_service_object = (*jni_env)->CallStaticObjectMethod(jni_env, sensor_service_class, sensor_service_getter);

    // Find SensorService method to get a list of Android Sensors
    sensor_service_method = (*jni_env)->GetMethodID(jni_env, sensor_service_class, "get_sensor_list", "()[Landroid/hardware/Sensor;");
    jobjectArray sensor_list = (*jni_env)->CallObjectMethod(jni_env, sensor_service_object, sensor_service_method);
    int sensor_list_count = (*jni_env)->GetArrayLength(jni_env, sensor_list);
//    LOGI("C says we have %i sensors", (*jni_env)->GetArrayLength(jni_env, sensor_list));

    // For each sensor,
    // call its info functions and save them to a python dict
    PyObject *py_sensor_list = PyList_New(sensor_list_count);
    PyObject *py_sensor_info;
    jclass sensor_class = (*jni_env)->FindClass(jni_env, "android/hardware/Sensor");
    int i;
    for (i = 0; i < sensor_list_count; i++) {
        py_sensor_info = PyDict_New();
        jobject sensor = (*jni_env)->GetObjectArrayElement(jni_env, sensor_list, i);

        // Find and call Sensor get name method
        jmethodID sensor_method = (*jni_env)->GetMethodID(jni_env, sensor_class, "getName", "()Ljava/lang/String;");
        jstring java_sensor_name = (*jni_env)->CallObjectMethod(jni_env, sensor, sensor_method);

        // Convert java string to c char
        const char *sensor_name = (*jni_env)->GetStringUTFChars(jni_env, java_sensor_name, 0);

        // Create dict entry for the name
        PyDict_SetItemString(py_sensor_info, "name", PyString_FromString(sensor_name));

        // Release the chars!!!!
        (*jni_env)->ReleaseStringUTFChars(jni_env, java_sensor_name, sensor_name);

        (*jni_env)->DeleteLocalRef(jni_env, sensor);
        (*jni_env)->DeleteLocalRef(jni_env, java_sensor_name);

        // XXX Todo do the same for all the other sensor info
        // ...

        // Add info dict to python sensor list
        PyList_SetItem(py_sensor_list, i, py_sensor_info);
    }

    // XXX: Only detach if AttachCurrentThread wasn't a no-op
    //(*cached_vm)->DetachCurrentThread(cached_vm);

    // Delete local references
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_object);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_list);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_class);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_class);

    return py_sensor_list;
}

/*
 * Python Extension(s) to get actual sensor values
 *  e.g. Accelerometer
 *
 * This is harder and/as there are multiple approaches,
 * - Do we want one function for all sensors, like in current Repy Sensor API? [1]
 * - Do we want to poll a sensor? (let's start with this)
 *  e.g.:
 *      start_sensing(TYPE_ACCELEROMETER)
 *      get_current_value()
 *      stop_sensing()
 * - Do we want to use callback functions? (cool but hard)
 *  e.g.:
 *       start_sensing(TYPE_ACCELEROMETER, fn_called_with_sensor_values)
 *       stop_sensing()
 * - Should we go OO, like Yocto Python API? [2] (not really pythonic, is it?)
 *  e.g.:
 *       sensor = find_sensor(TYPE_ACCELEROMETER)
 *       sensor.start()
 *       sensor.getValue()
 *       sensor.stop()
 *
 * In any case we need to think of,
 *  - properly creating, starting, stopping, destroying sensors and sensor listener.
 *  - multiple sensors for one type.
 *  - multiple vessels concurrently accessing a sensor.
 *
 * [1] https://sensibilitytestbed.com/projects/project/wiki/sensors#Sensors
 * [2] http://www.yoctopuce.com/EN/products/yocto-meteo/doc/METEOMK1.usermanual.html#CHAP14
 *
 */

int _start_or_stop_sensing(const char *sensor_service_method_name, int sensor_type) {
    JNIEnv* jni_env;
    jclass sensor_service_class;
    jmethodID sensor_service_getter;
    jobject sensor_service_object;
    jmethodID sensor_service_method;

    // Use the cached JVM pointer to get a new environment
    (*cached_vm)->AttachCurrentThread(cached_vm, &jni_env, NULL);
    // Find SensorService class and get singleton instance
    sensor_service_class = (*jni_env)->FindClass(jni_env, "com/snakei/SensorService");
    sensor_service_getter = (*jni_env)->GetStaticMethodID(jni_env, sensor_service_class, "getInstance", "()Lcom/snakei/SensorService;");
    sensor_service_object = (*jni_env)->CallStaticObjectMethod(jni_env, sensor_service_class, sensor_service_getter);

    // XXX: We'll need to pass an argument, on which sensor we want to start or stop sensing
    sensor_service_method = (*jni_env)->GetMethodID(jni_env, sensor_service_class, sensor_service_method_name, "(I)I");
    int success = (int) (*jni_env)->CallBooleanMethod(jni_env, sensor_service_object, sensor_service_method, sensor_type);
    // XXX: Only detach if AttachCurrentThread wasn't a no-op
    //(*cached_vm)->DetachCurrentThread(cached_vm);

    // Delete local references
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_object);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_class);

    return success;
}

PyObject* _get_sensor_values(const char *sensor_service_method_name) {
//    LOGI("Get sensor values...");
    JNIEnv* jni_env;
    jclass sensor_service_class;
    jmethodID sensor_service_getter;
    jobject sensor_service_object;
    jmethodID sensor_service_method;

    // Use the cached JVM pointer to get a new environment
    (*cached_vm)->AttachCurrentThread(cached_vm, &jni_env, NULL);

    // Find SensorService class and get singleton instance
    sensor_service_class = (*jni_env)->FindClass(jni_env, "com/snakei/SensorService");
    sensor_service_getter = (*jni_env)->GetStaticMethodID(jni_env, sensor_service_class, "getInstance", "()Lcom/snakei/SensorService;");
    sensor_service_object = (*jni_env)->CallStaticObjectMethod(jni_env, sensor_service_class, sensor_service_getter);

    sensor_service_method = (*jni_env)->GetMethodID(jni_env, sensor_service_class, sensor_service_method_name, "()[D");
    jdoubleArray sensor_values = (jdoubleArray) (*jni_env)->CallObjectMethod(jni_env, sensor_service_object, sensor_service_method);

    if (sensor_values == NULL) {
        (*jni_env)->DeleteLocalRef(jni_env, sensor_service_object);
        (*jni_env)->DeleteLocalRef(jni_env, sensor_values);
        (*jni_env)->DeleteLocalRef(jni_env, sensor_service_class);
        LOGI("NULL");
        Py_RETURN_NONE;
    }

    int sensor_values_cnt = (*jni_env)->GetArrayLength(jni_env, sensor_values);
    jdouble *sensor_values_ptr = (*jni_env)->GetDoubleArrayElements(jni_env, sensor_values, 0);

    PyObject *py_sensor_values = PyList_New(sensor_values_cnt);
    int i = 0;
    for (i = 0; i < sensor_values_cnt; i++) {
        PyList_SetItem(py_sensor_values, i, Py_BuildValue("d", sensor_values_ptr[i]));
    }

    (*jni_env)->ReleaseDoubleArrayElements(jni_env, sensor_values, sensor_values_ptr, 0);

    // XXX: Only detach if AttachCurrentThread wasn't a no-op
    //(*cached_vm)->DetachCurrentThread(cached_vm);

    // Delete local references
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_object);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_values);
    (*jni_env)->DeleteLocalRef(jni_env, sensor_service_class);

    return py_sensor_values;
}

PyObject* sensor_get_acceleration(PyObject *self) {
    return _get_sensor_values("getAcceleration");
}
PyObject* sensor_get_ambient_temperature(PyObject *self) {
    return _get_sensor_values("getAmbientTemperature");
}
PyObject* sensor_get_game_rotation_vector(PyObject *self) {
    return _get_sensor_values("getGameRotationVector");
}
PyObject* sensor_get_geomagnetic_rotation_vector(PyObject *self) {
    return _get_sensor_values("getGeomagneticRotationVector");
}
PyObject* sensor_get_gravity(PyObject *self) {
    return _get_sensor_values("getGravity");
}
PyObject* sensor_get_gyroscope(PyObject *self) {
    return _get_sensor_values("getGyroscope");
}
PyObject* sensor_get_gyroscope_uncalibrated(PyObject *self) {
    return _get_sensor_values("gyroscope_uncalibrated_event");
}
PyObject* sensor_get_heart_rate(PyObject *self) {
    return _get_sensor_values("getHeartRate");
}
PyObject* sensor_get_light(PyObject *self) {
    return _get_sensor_values("getLight");
}
PyObject* sensor_get_linear_acceleration(PyObject *self) {
    return _get_sensor_values("getLinearAcceleration");
}
PyObject* sensor_get_magnetic_field(PyObject *self) {
    return _get_sensor_values("getMagneticField");
}
PyObject* sensor_get_magnetic_field_uncalibrated(PyObject *self) {
    return _get_sensor_values("getMagneticFieldUncalibrated");
}
PyObject* sensor_get_pressure(PyObject *self) {
    return _get_sensor_values("getPressure");
}
PyObject* sensor_get_proximity(PyObject *self) {
    return _get_sensor_values("getProximity");
}
PyObject* sensor_get_relative_humidity(PyObject *self) {
    return _get_sensor_values("getRelativeHumidity");
}
PyObject* sensor_get_rotation_vector(PyObject *self) {
    return _get_sensor_values("getRotationVector");
}
PyObject* sensor_get_step_counter(PyObject *self) {
    return _get_sensor_values("getStepCounter");
}

/*
 * Start a sensor by registering a SensorEventHandler in Java
 */
int sensor_start_sensing(int sensor_type) {
//    LOGI("Let's fire the sensor up...");
    return _start_or_stop_sensing("start_sensing", sensor_type);
}
/*
 * Stop a sensor by unregistering a SensorEventHandler in Java
 */
int sensor_stop_sensing(int sensor_type) {
//    LOGI("Let's shut down the sensor...");
    return _start_or_stop_sensing("stop_sensing", sensor_type);
}