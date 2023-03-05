#ifndef REALTIME_IIR
#   define REALTIME_IIR

typedef unsigned int uint32;

template<typename T>
class RT_IIR {
    private:
        typedef struct node {
            T value;
            struct node *next;
        } node;
        node *in, *out;
        T *b, *a; // coefficients eg: from Matlab coefficients
        T alpha;
        T last_output;
        uint32 n_a, n_b;
    public:
        RT_IIR (uint32 n_b, uint32 n_a, T *b, T *a, T init_value = 0);
        T add (T value);
        T compute( );
        T getLastOutput( );
};

template <typename T>
RT_IIR<T>::RT_IIR (uint32 n_b, uint32 n_a, T *b, T *a, T init_value) {
    node *in;
    node *out;
    last_output = 0;
    this->n_a = n_a - 1; // a[0] = alpha
    this->n_b = n_b;

    this->b = (T*) malloc(sizeof(T) * n_b);
    memcpy(this->b, b, sizeof(T) * n_b);
    this->a = (T*) malloc(sizeof(T) * this->n_a);
    memcpy(this->a, &(a[1]), sizeof(T) * this->n_a);
    this->alpha = a[0];

    // init input circular list
    in = (node*) malloc(sizeof(node));
    this->in = in;
    in->value = init_value;
    for (uint32 i = 1; i < this->n_b; i++) {
        in->next = (node*) malloc(sizeof(node));
        in = in->next;
        in->value = init_value;
    }
    in->next = this->in;
    // init output circular list
    out = (node*) malloc(sizeof(node));
    this->out = out;
    out->value = init_value;
    for (uint32 i = 1; i < this->n_a; i++) {
        out->next = (node*) malloc(sizeof(node));
        out = out->next;
        out->value = init_value;
    }
    out->next = this->out;
}

template <typename T>
T RT_IIR<T>::add (T value) {
    // append the last input value and circle
    this->in->value = value;
    this->in = this->in->next;
    // append last output value
    this->out->value = last_output;
    this->out = this->out->next;
    // compute filtering and return the filtered value
    this->last_output = compute();
    return this->last_output;
}

template <typename T>
T RT_IIR<T>::compute( ) {
    T sum_b = 0;
    T sum_a = 0;
    T filtered;
    uint32 w_index, counter = 0;
    // sum the old input values
    void *_ = this->in;
    do {
        w_index = this->n_b - counter - 1;
        sum_b += this->in->value * this->b[w_index];
        this->in = this->in->next;
        counter ++;
    } while ((node*) _ != this->in);
    // sum the old output values
    _ = this->out;
    counter = 0;
    do {
        w_index = this->n_a - counter - 1;
        sum_a += this->out->value * this->a[w_index];
        this->out = this->out->next;
        counter ++;
    } while ((node*) _ != this->out);
    filtered = (sum_b - sum_a) / this->alpha;
    return filtered;
}

template <typename T>
T RT_IIR<T>::getLastOutput( ) {
    return this->last_output;
}
#endif // REALTIME_IIR
