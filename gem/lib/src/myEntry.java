import java.util.Map.Entry;


public class myEntry<K, V> implements Entry<K, V>{
	
	private K key;
	private V value;
	
	public myEntry(K k, V v){
		key = k;
		value = v;
	}

	@Override
	public K getKey() {
		return key;
	}

	@Override
	public V getValue() {
		return value;
	}

	@Override
	public V setValue(V value) {
		this.value = value;
		return value;
	}

}